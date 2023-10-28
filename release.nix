{ nixpkgs ? <nixpkgs> }:

let
  pkgsFromSystem = system: (import nixpkgs { config = {}; inherit system; });
  makeJob = (s: { ${s} = (pkgsFromSystem s).callPackage ./default.nix {}; });
  nativePkgs = import nixpkgs {};
  merge = a: b: a // b;
  mergeList = builtins.foldl' merge {};
  makeJobs = systems: mergeList (map makeJob systems);
  makeRPM = system: diskImageFun: extraPackages: with import nixpkgs { inherit system; };
  releaseTools.rpmBuild rec {
    name = "toxvpn-rpm";
    src = ./.;
    diskImage = (diskImageFun vmTools.diskImageFuns) { inherit extraPackages; };
    memSize = 1024;
  };
in { toxvpn = makeJobs [ "x86_64-linux" /*"x86_64-darwin"*/ ]; }
