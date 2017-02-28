{ nixpkgs ? <nixpkgs> }:

let
  pkgsFromSystem = system: (import nixpkgs { config = {}; inherit system; });
  makeJob = (s: { ${s} = (pkgsFromSystem s).callPackage ./default.nix {}; });
  nativePkgs = import nixpkgs {};
  merge = a: b: a // b;
  mergeList = builtins.foldl' merge {};
  makeJobs = systems: mergeList (map makeJob systems);
in { toxvpn = makeJobs [ "x86_64-linux" "x86_64-darwin" ]; }
