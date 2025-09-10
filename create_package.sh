#!/bin/bash

# Set variables based on the Makefile
SRCDIR="src"
HEADERDIR="inc"
OBJDIR="obj"
BINDIR="bin"
PKGDIR="pkg"
PKGBINDIR="pkgbin"
RELEASE="release"
DEBUG="debug"
EXENAME="sdgyrodsu"
PKGNAME="SteamDeckGyroDSUSetup"
SYMRELEASE="launch"
SYMDEBUG="launchdebug"
SRCEXT="cpp"
OBJEXT="o"
CC="g++"
CPPSTD="c++2a"
ADDPARS=""
ADDRELEASEPARS="-O3"
ADDDEBUGPARS="-g"
ADDLIBS="-pthread -lncurses -lsystemd -lhidapi-hidraw"
INSTALLSCRIPT="install.sh"
PREPARESCRIPT="scripts/prepare.sh"

# Function to build the release executable (replicates the 'release' target)
build_release() {
  mkdir -p "$BINDIR/$RELEASE"
  find "$OBJDIR/$RELEASE" -name "*.$OBJEXT" -print0 | xargs -0 "$CC" $ADDLIBS -o "$BINDIR/$RELEASE/$EXENAME"
  ln -sf "$EXENAME" "$BINDIR/$RELEASE/$SYMRELEASE"
}

# Function to prepare the package files (replicates the 'preparepkg' target)
prepare_package() {
  mkdir -p "$PKGBINDIR/$PKGNAME"
  cp "$BINDIR/$RELEASE/$EXENAME" "$PKGBINDIR/$PKGNAME"
  cp -r "$PKGDIR"/* "$PKGBINDIR/$PKGNAME"
}

# Function to create the package (replicates the 'createpkg' target)
create_package() {
  cd "$PKGBINDIR"
  zip -r "$PKGNAME.zip" "$PKGNAME"
  echo "Binary package '$PKGBINDIR/$PKGNAME.zip' created."
}

# Main execution

# Build the release executable
build_release

# Prepare the package files
prepare_package

# Create the package
create_package

echo "Package creation complete."
