#!/bin/bash
# WeatherClockGTK Linux Build Script

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default values
SKIP_CLEAN=false
SKIP_RUN=false
BUILD_DIR="build"
DEPLOY=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -SkipClean|--skip-clean)
            SKIP_CLEAN=true
            shift
            ;;
        -SkipRun|--skip-run)
            SKIP_RUN=true
            shift
            ;;
        -Deploy|--deploy)
            DEPLOY=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  -SkipClean, --skip-clean    Skip cleaning build directory"
            echo "  -SkipRun, --skip-run         Build but don't run"
            echo "  -Deploy, --deploy           Create deployment package"
            echo "  -h, --help                  Show this help message"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

echo "=== WeatherClockGTK Build Script ==="
echo ""

# Check for required tools
if ! command -v cmake &> /dev/null; then
    echo -e "${RED}Error: cmake is not installed${NC}"
    echo "Install it with: sudo apt-get install cmake"
    exit 1
fi

if ! command -v pkg-config &> /dev/null; then
    echo -e "${RED}Error: pkg-config is not installed${NC}"
    echo "Install it with: sudo apt-get install pkg-config"
    exit 1
fi

# Check for required packages
echo "Checking dependencies..."
MISSING_DEPS=()

if ! pkg-config --exists gtk4; then
    MISSING_DEPS+=("libgtk-4-dev")
fi

if ! pkg-config --exists libsoup-3.0; then
    MISSING_DEPS+=("libsoup-3.0-dev")
fi

if ! pkg-config --exists json-glib-1.0; then
    MISSING_DEPS+=("json-glib-dev")
fi

if [ ${#MISSING_DEPS[@]} -ne 0 ]; then
    echo -e "${YELLOW}Missing dependencies:${NC}"
    for dep in "${MISSING_DEPS[@]}"; do
        echo "  - $dep"
    done
    echo ""
    echo "Install them with:"
    echo "  sudo apt-get install ${MISSING_DEPS[*]} build-essential cmake pkg-config"
    exit 1
fi

echo -e "${GREEN}All dependencies found${NC}"
echo ""

# Clean build directory
if [ "$SKIP_CLEAN" = false ]; then
    echo "Cleaning build directory..."
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
    fi
    echo -e "${GREEN}Build directory cleaned${NC}"
else
    echo "Skipping clean (using existing build directory)"
fi

echo ""

# Create build directory
echo "Creating build directory..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo "Configuring with CMake..."
cmake ..
if [ $? -ne 0 ]; then
    echo -e "${RED}CMake configuration failed${NC}"
    exit 1
fi
echo -e "${GREEN}CMake configuration successful${NC}"
echo ""

# Build
echo "Building project..."
cmake --build . -j$(nproc)
if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed${NC}"
    exit 1
fi
echo -e "${GREEN}Build successful!${NC}"
echo ""

# Find executable
EXECUTABLE=""
if [ -f "weatherclock" ]; then
    EXECUTABLE="weatherclock"
elif [ -f "weatherclock.exe" ]; then
    EXECUTABLE="weatherclock.exe"
fi

if [ -z "$EXECUTABLE" ]; then
    echo -e "${RED}Error: Executable not found${NC}"
    exit 1
fi

echo "Found executable: $EXECUTABLE"
echo ""

# Deploy stage
if [ "$DEPLOY" = true ]; then
    echo "=== Deployment Stage ==="
    DEPLOY_DIR="../deploy"
    
    echo "Creating deployment directory..."
    rm -rf "$DEPLOY_DIR"
    mkdir -p "$DEPLOY_DIR"
    
    echo "Copying executable..."
    cp "$EXECUTABLE" "$DEPLOY_DIR/"
    
    echo "Creating run script..."
    cat > "$DEPLOY_DIR/run.sh" << 'EOF'
#!/bin/bash
cd "$(dirname "$0")"
./weatherclock "$@"
EOF
    chmod +x "$DEPLOY_DIR/run.sh"
    
    echo -e "${GREEN}Deployment created in: $DEPLOY_DIR${NC}"
    
    # Calculate size
    SIZE=$(du -sh "$DEPLOY_DIR" | cut -f1)
    echo "Deployment size: $SIZE"
    echo ""
fi

# Run
if [ "$SKIP_RUN" = false ] && [ "$DEPLOY" = false ]; then
    echo "Launching application..."
    echo ""
    cd ..
    "./$BUILD_DIR/$EXECUTABLE" "$@"
fi

echo ""
echo -e "${GREEN}=== Done ===${NC}"

