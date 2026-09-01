#!/usr/bin/env bash
# 构建个人 pacman 仓库：
#   1. 用 PKGBUILD 打出 noctalia 包（默认从本地 checkout 构建，避免 GitHub
#      网络抖动；GH=1 时改用 GitHub 远端，用于在干净机器上复现）。
#   2. repo-add 生成仓库数据库，输出到 gh-pages/ 布局。
#   3. 打印需要推送到 TNXGLab/arch-repo（GitHub Pages）的 git 命令。
repo_dir="$(cd "$(dirname "$0")" && pwd)/../.."

work_dir="$(mktemp -d /tmp/noctalia-packaging.XXXXXX)"
out_dir="$repo_dir/packaging/arch/gh-pages"
arch=x86_64

cp "$repo_dir/packaging/arch/PKGBUILD" "$work_dir/"
cd "$work_dir"

if [[ "${GH:-0}" == "1" ]]; then
  # PKGBUILD 默认 source 即 GitHub，直接 makepkg。
  makepkg -f --noconfirm
else
  # 本地构建：把 source 指向本地 checkout（file:// 协议），零网络依赖。
  sed -i "s#git+https://github.com/TNXGLab/noctalia.git#git+file://$repo_dir#" PKGBUILD
  makepkg -f --noconfirm
fi

mkdir -p "$out_dir/$arch"
cp -f noctalia-*.pkg.tar.zst "$out_dir/$arch/"
cd "$out_dir/$arch"
repo-add noctalia.db.tar.gz noctalia-*.pkg.tar.zst

cat <<EOF

✅ 包已构建: $out_dir/$arch/
发布到 GitHub Pages（仓库: TNXGLab/arch-repo, 分支: gh-pages）:

  cd $out_dir
  git init -b gh-pages .
  git add -A
  git commit -m "noctalia $(ls noctalia-$arch/*.pkg.tar.zst | head -1 | xargs basename)"
  git remote add origin https://github.com/TNXGLab/arch-repo.git
  git push -u origin gh-pages --force

首次推送前需在 GitHub 上创建空的 public 仓库 TNXGLab/arch-repo。
EOF
