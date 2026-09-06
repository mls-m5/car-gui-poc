#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)
image_name=${CAR_GUI_WEB_IMAGE:-car-gui-web}
output_dir=${CAR_GUI_WEB_OUTPUT:-$project_dir/build-web-container}
public_dir=${CAR_GUI_WEB_PUBLIC:-$output_dir/public}
port=${CAR_GUI_WEB_PORT:-8080}
serve=false

usage() {
    cat <<EOF
Usage: $0 [--serve]

Build the Emscripten application in an OCI image using Podman, or Docker when
Podman is unavailable. The generated browser files are copied to:
  $output_dir
and:
  $public_dir

Options:
  --serve  Run the resulting nginx image on http://localhost:$port
  -h       Show this help

Environment:
  CONTAINER_ENGINE     Override the container command (podman or docker)
  CAR_GUI_WEB_IMAGE    Image name (default: car-gui-web)
  CAR_GUI_WEB_OUTPUT   Build artifact directory
  CAR_GUI_WEB_PUBLIC   Public web directory (default: <output>/public)
  CAR_GUI_WEB_PORT     Host port used by --serve (default: 8080)
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --serve) serve=true ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown argument: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

if [ -n "${CONTAINER_ENGINE:-}" ]; then
    engine=$CONTAINER_ENGINE
elif command -v podman >/dev/null 2>&1; then
    engine=podman
elif command -v docker >/dev/null 2>&1; then
    engine=docker
else
    echo "Neither Podman nor Docker is installed." >&2
    exit 1
fi

printf 'Building %s with %s...\n' "$image_name" "$engine"
"$engine" build --file "$project_dir/Dockerfile.emscripten" \
    --tag "$image_name" "$project_dir"

container_id=$("$engine" create "$image_name")
cleanup() {
    "$engine" rm -f "$container_id" >/dev/null 2>&1 || true
}
trap cleanup EXIT INT TERM

rm -rf "$output_dir"
mkdir -p "$output_dir"
"$engine" cp "$container_id:/usr/share/nginx/html/." "$output_dir/"
cleanup
trap - EXIT INT TERM

install -m 755 "$script_dir/serve-web.sh" "$output_dir/serve.sh"
mkdir -p "$public_dir"
for artifact in index.html car-gui.html car-gui.js car-gui.wasm car-gui.data; do
    cp "$output_dir/$artifact" "$public_dir/$artifact"
done

printf '\nWeb build written to %s\n' "$output_dir"
printf 'Web build also copied to %s\n' "$public_dir"
printf 'Local server script written to %s/serve.sh\n' "$output_dir"
printf 'Image created: %s\n' "$image_name"

if [ "$serve" = true ]; then
    printf 'Serving at http://localhost:%s (press Ctrl-C to stop)\n' "$port"
    exec "$engine" run --rm -p "$port:80" "$image_name"
else
    printf 'Run %s --serve to build and serve it.\n' "$0"
fi
