{ stdenv, clangStdenv, lib, fetchFromGitHub
, cmake, libsodium, systemd, nlohmann_json, libtoxcore, libcap, zeromq
}:

with rec {
  enableDebugging = false;

  libtoxcoreLocked = (libtoxcore.override { libconfig = null; }).overrideAttrs(old: {
    name = "libtoxcore-20250101";

    src = fetchFromGitHub {
      owner  = "cleverca22";
      repo   = "toxcore";
      rev    = "e5a5c75eb889be932d6c14f3edcfaf2077fba231";
      hash   = "sha256-WLHRW+2Phxv1U3qxb9lQSJhGQ/573O+QDkTPUyjivnc=";
      fetchSubmodules = true;
    };

    dontStrip = enableDebugging;
    cmakeFlags = [
      "-DDHT_BOOTSTRAP=ON"
      "-DBOOTSTRAP_DAEMON=OFF"
      "-DENABLE_SHARED=ON"
      "-DENABLE_STATIC=ON"
    ];
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
