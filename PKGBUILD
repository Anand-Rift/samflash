pkgname=samflash
pkgver=0.1.0
pkgrel=1
pkgdesc="A GUI samsung flashing tool (odin4 + heimdall)"
arch=('x86_64')
url="https://github.com/Anand-Rift/samflash"
license=('GPL3')
depends=('qt6-base' 'heimdall')
optdepends=('odin4: Odin mode (AUR)')
makedepends=('cmake')
source=("$pkgname-$pkgver.tar.gz::$url/archive/refs/tags/v$pkgver.tar.gz")
sha256sums=('SKIP')

build() {
  cmake -B build -S "$pkgname-$pkgver" \
    -DCMAKE_BUILD_TYPE=None \
    -DCMAKE_INSTALL_PREFIX=/usr
  cmake --build build
}

package() {
  DESTDIR="$pkgdir" cmake --install build
}
