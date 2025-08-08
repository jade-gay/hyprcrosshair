pkgname=hyprcrosshair
pkgver=0.1.0
pkgrel=1
pkgdesc="A simple crosshair overlay for Hyprland"
arch=('x86_64')
url="https://github.com/jade-gay/hyprcrosshair"
license=('MIT')
depends=('gtk4' 'libadwaita' 'gtk4-layer-shell')
makedepends=('meson' 'ninja' 'gcc')
source=("git+https://github.com/jade-gay/hyprcrosshair.git")
sha256sums=('SKIP')

build() {
  cd "$srcdir/$pkgname"
  meson setup build
  meson compile -C build
}

package() {
  cd "$srcdir/$pkgname"
  meson install -C build --destdir="$pkgdir"
}
