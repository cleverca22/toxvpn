{ pkgs ? (with import <nixpkgs> {}) }:

{
  toxvpn = pkgs.callPackage ./default.nix {};
}
