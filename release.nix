{ nixpkgs ? <nixpkgs> }:

let
  fn = system:
    let
      pkgs = import nixpkgs { config = {}; inherit system; };
    in {
      toxvpn.${system} = pkgs.callPackage ./default.nix {};
    };
  nativePkgs = import nixpkgs {};
  makeJobs = nativePkgs.lib.foldl (total: next: total // (fn next)) {};
in makeJobs [ "x86_64-linux" "x86_64-darwin" ]
