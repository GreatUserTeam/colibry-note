# Maintainer: Colibry OS contributors
pkgname=colibry-notes
pkgver=0.2.0
pkgrel=1
pkgdesc="Colibry OS: Plain-text notes, one file per note"
arch=('x86_64')
url="https://github.com/GreatUserTeam/colibry-notes"
license=('MIT')
depends=('vulkan-icd-loader' 'libx11' 'libxi')
makedepends=('cmake' 'vulkan-headers' 'git')
source=("$pkgname-$pkgver.tar.gz::$url/archive/v$pkgver.tar.gz")
sha256sums=('SKIP')

# Raven (Apache-2.0) is not in the AUR: CMake downloads it at a pinned commit when no installed Raven is found.

build() {
    cmake -B build -S "$srcdir/$pkgname-$pkgver" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
        -DCOLIBRY_BUILD_TESTS=OFF
    cmake --build build
}

check() {
    cmake -B build-test -S "$srcdir/$pkgname-$pkgver" -DCOLIBRY_BUILD_TESTS=ON
    cmake --build build-test
    (cd build-test && ctest --output-on-failure)
}

package() {
    DESTDIR="$pkgdir" cmake --install "$srcdir/build"
    install -Dm644 "$srcdir/$pkgname-$pkgver/LICENSE" "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
