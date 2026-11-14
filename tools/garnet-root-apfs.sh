#!/usr/bin/env bash
# Rebuild garnet's root.apfs (md1, mounted at /redwood) from the device repo's image with this build's files in it.
#
# usage: tools/garnet-root-apfs.sh BASE.apfs OUT.apfs [--plugin BINARY] [--plugin-plist PLIST] [--osmesa DYLIB]
#                                  [--file SRC=PATH]... [--manifest FILE]
#
#   --plugin        hakios-metal's Metal plug-in (userland/src/HakiosMetalCA.m + HakiosMetalAlias.m), installed as
#                   lib/RedwoodMetal.bundle/RedwoodMetal: the bundle the system image links from
#                   /System/Library/Extensions/RedwoodMetal.bundle, and the one the kernel names (MetalPluginName)
#   --plugin-plist  its Info.plist (userland/src/RedwoodMetal-garnet.plist)
#   --osmesa        Mesa's libOSMesa.8.dylib (tools/mk-mesa.sh), as lib/libOSMesa.8.dylib: the GL bridge's library
#   --file          SRC installed at PATH in the image (mode 755 if SRC is executable, else 644; may repeat): the
#                   service stub (userland/src/sleepd_stub.c) and its launchd job
#   --manifest      write OUT's file list with sizes and md5s here
#
# Everything else of BASE is kept as it is (the injected libraries, launchd's jobs, the firmware, the wallpapers).
# The image is made as userland/build.sh makes it: mkapfs.py -z --plain fw, one volume named as BASE's ("root"),
# everything root:wheel (BASE's are). It is then read back and the replaced files compared byte for byte.
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
APFS="python3 $here/apfs/apfs.py"
MKAPFS="python3 $here/apfs/mkapfs.py"

[ $# -ge 2 ] || { sed -n 2,16p "$0" >&2; exit 2; }
base=$1 out=$2
shift 2
plugin= plist= osmesa= manifest=
files=()
while [ $# -gt 0 ]; do
	case $1 in
	--plugin) plugin=$2; shift 2 ;;
	--plugin-plist) plist=$2; shift 2 ;;
	--osmesa) osmesa=$2; shift 2 ;;
	--manifest) manifest=$2; shift 2 ;;
	--file) files+=("$2"); shift 2 ;;
	*) echo "garnet-root-apfs.sh: unknown option $1" >&2; exit 2 ;;
	esac
done

work=$(mktemp -d "${TMPDIR:-/tmp}/garnet-root.XXXXXX")
trap 'chmod -R u+w "$work" 2>/dev/null; rm -rf "$work"' EXIT
tree=$work/root

volname=$($APFS info "$base" | sed -n 's/^ *\[0\] \([^ ]*\) .*/\1/p' | head -1)
[ -n "$volname" ] || { echo "garnet-root-apfs.sh: no volume in $base" >&2; exit 1; }
$APFS extract "$base" "$tree"

replaced=()
if [ -n "$plugin" ]; then
	b=$tree/lib/RedwoodMetal.bundle
	# (BASE has it as a directory with HakiosMetal -> RedwoodMetal in it, and lib/HakiosMetal.bundle -> RedwoodMetal.bundle)
	[ -L "$b" ] && rm -f "$b"
	mkdir -p "$b"
	rm -f "$b/RedwoodMetal"
	install -m 755 "$plugin" "$b/RedwoodMetal"
	ln -sfn RedwoodMetal "$b/HakiosMetal"
	[ -e "$tree/lib/HakiosMetal.bundle" ] || ln -sfn RedwoodMetal.bundle "$tree/lib/HakiosMetal.bundle"
	replaced+=("$plugin=/lib/RedwoodMetal.bundle/RedwoodMetal")
	if [ -n "$plist" ]; then
		install -m 644 "$plist" "$b/Info.plist"
		replaced+=("$plist=/lib/RedwoodMetal.bundle/Info.plist")
	fi
	# HakiosMetalCA.m reads CoreAnimation's merged library from share/ca-share (HM_CA_METALLIB); BASE has it in share/
	if [ -f "$tree/share/QuartzCore-default.metallib" ] && [ ! -e "$tree/share/ca-share/QuartzCore-default.metallib" ]; then
		mkdir -p "$tree/share/ca-share"
		ln -s ../QuartzCore-default.metallib "$tree/share/ca-share/QuartzCore-default.metallib"
	fi
	# ... and the compiled-stage cache's seed directory (HM_CACHE_SEED): empty, the device fills /var/db's
	mkdir -p "$tree/share/hakios-metal-cache"
fi
if [ -n "$osmesa" ]; then
	rm -f "$tree/lib/libOSMesa.8.dylib"
	install -m 755 "$osmesa" "$tree/lib/libOSMesa.8.dylib"
	replaced+=("$osmesa=/lib/libOSMesa.8.dylib")
fi

for r in "${files[@]}"; do
	src=${r%%=*} path=${r#*=}
	case $path in /*) ;; *) echo "garnet-root-apfs.sh: --file $r: PATH must be absolute" >&2; exit 2 ;; esac
	mkdir -p "$tree$(dirname "$path")"
	rm -f "$tree$path"
	if [ -x "$src" ]; then install -m 755 "$src" "$tree$path"; else install -m 644 "$src" "$tree$path"; fi
	replaced+=("$src=$path")
done

rm -f "$out"
$MKAPFS -z --plain fw -o "$out" "$volname=$tree" | tail -1

# read back: the container is whole, and what was put in is what is there
$APFS census "$out" | sed 's/^/  /'
for r in "${replaced[@]}"; do
	src=${r%%=*} path=${r#*=}
	a=$(md5sum < "$src" | cut -c1-32)
	b=$($APFS cat "$out" "$path" | md5sum | cut -c1-32)
	if [ "$a" != "$b" ]; then
		echo "garnet-root-apfs.sh: $path in $out is not $src ($b, want $a)" >&2
		exit 1
	fi
	echo "  $path  $a"
done

if [ -n "$manifest" ]; then
	(cd "$tree" && find . -mindepth 1 | LC_ALL=C sort | while read -r f; do
		if [ -L "$f" ]; then
			printf '%s -> %s\n' "${f#.}" "$(readlink "$f")"
		elif [ -f "$f" ]; then
			printf '%s %s %s\n' "${f#.}" "$(stat -c %s "$f")" "$(md5sum < "$f" | cut -c1-32)"
		fi
	done) > "$manifest"
fi
echo "garnet-root-apfs.sh: $out ($(stat -c %s "$out") bytes, volume $volname)"
