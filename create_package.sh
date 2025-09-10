#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e

# --- Configuration ---
SRCDIR="src"
HEADERDIR="inc"
OBJDIR="obj"
BINDIR="bin"
PKGDIR="pkg"
PKGBINDIR="pkgbin"
RELEASE="release"
EXENAME="sdgyrodsu"
PKGNAME="SteamDeckGyroDSUSetup"
SYMRELEASE="launch"
SRCEXT="cpp"
OBJEXT="o"
CC="g++"
CPPSTD="c++2a"
ADDRELEASEPARS="-O3"

# Common libraries
# The -lsystemd is now explicitly handled in the linking step for clarity and correctness
ADDLIBS="-pthread -lncurses -lhidapi-hidraw"

# Use static libraries for better portability. Some libraries are not fully static.
STATIC_FLAGS="-static-libgcc -static-libstdc++"

# Add the include path to the compiler flags
INCLUDE_FLAG="-I$HEADERDIR"

# --- Functions ---

# Function to build the release executable
build_release() {
  echo "Building release executable..."
  mkdir -p "$OBJDIR/$RELEASE" "$BINDIR/$RELEASE"

  SOURCES=$(find "$SRCDIR" -name "*.${SRCEXT}")
  if [ -z "$SOURCES" ]; then
    echo "Error: No source files found in '$SRCDIR'."
    exit 1
  fi

  # Compile source files
  OBJECTS=""
  for source_file in $SOURCES; do
    object_file=$(echo "$source_file" | sed "s|^$SRCDIR|$OBJDIR/$RELEASE|" | sed "s|\.${SRCEXT}|\.${OBJEXT}|")
    mkdir -p "$(dirname "$object_file")"
    "$CC" -c "$source_file" -o "$object_file" -std="$CPPSTD" $ADDRELEASEPARS $INCLUDE_FLAG $STATIC_FLAGS
    OBJECTS="$OBJECTS $object_file"
  done

  # Link objects to create the final executable, including all necessary libraries
  "$CC" $OBJECTS $STATIC_FLAGS $ADDLIBS -lsystemd -o "$BINDIR/$RELEASE/$EXENAME"

  # Create symbolic link
  ln -sf "$EXENAME" "$BINDIR/$RELEASE/$SYMRELEASE"
  echo "Release executable built successfully."
}

# Function to prepare the package files
prepare_package() {
  echo "Preparing package files..."
  mkdir -p "$PKGBINDIR/$PKGNAME"

  if [ ! -f "$BINDIR/$RELEASE/$EXENAME" ]; then
    echo "Error: Release executable not found. Did the build fail?"
    exit 1
  fi
  cp "$BINDIR/$RELEASE/$EXENAME" "$PKGBINDIR/$PKGNAME"

  if [ -d "$PKGDIR" ]; then
    cp -r "$PKGDIR"/* "$PKGBINDIR/$PKGNAME"
  else
    echo "Warning: No additional package files found in '$PKGDIR'."
  fi
  echo "Package files prepared."
}

# Function to create the package
create_package() {
  echo "Creating the package..."
  if [ ! -d "$PKGBINDIR/$PKGNAME" ]; then
    echo "Error: Prepared package directory not found. Did the previous step fail?"
    exit 1
  fi
  cd "$PKGBINDIR"
  zip -r "$PKGNAME.zip" "$PKGNAME"
  echo "Binary package '$PKGBINDIR/$PKGNAME.zip' created successfully."
}

# --- Main execution ---
build_release
prepare_package
create_package

echo "Package creation complete."
