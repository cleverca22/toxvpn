{ stdenv, clangStdenv, lib, fetchFromGitHub
, cmake, libsodium, systemd, nlohmann_json, libtoxcore, libcap, zeromq
}:

with rec {
  enableDebugging = true;

  libtoxcoreLocked = lib.overrideDerivation (libtoxcore.override { libconfig = null; }) (old: {
    name = "libtoxcore-20160907";

    src = fetchFromGitHub {
      owner  = "TokTok";
      repo   = "c-toxcore";
      rev    = "ef7058422eec1c8b90208bb3522fce28374feb58";
      sha256 = "1fv8y80n0zc8886qa46m5bzyqy1d3vg88jjkjdssc7bwlgkcm383";
    };

    dontStrip = enableDebugging;
  });

  systemdOrNull = if stdenv.system == "x86_64-darwin" then null else systemd;

  if_systemd = lib.optional (systemdOrNull != null);
};

stdenv.mkDerivation {
  name = "toxvpn-git";

  src = ./.;

  dontStrip = enableDebugging;

  NIX_CFLAGS_COMPILE = if enableDebugging then [ "-ggdb -Og" ] else [];

  buildInputs = lib.concatLists [
    [ cmake libtoxcoreLocked nlohmann_json libsodium ]
    (if_systemd systemd)
    (lib.optional (stdenv.system != "x86_64-darwin") libcap)
    (lib.optional (zeromq != null) zeromq)
  ];

  cmakeFlags = (if_systemd [ "-DSYSTEMD=1" ]) ++ (lib.optional (zeromq != null) "-DZMQ=1");

  meta = with lib; {
    description = "A tool for making tunneled connections over Tox";
    homepage    = "https://github.com/cleverca22/toxvpn";
    license     = licenses.gpl3;
    maintainers = with maintainers; [ cleverca22 obadz ];
    platforms   = platforms.linux ++ platforms.darwin;
  };
}
