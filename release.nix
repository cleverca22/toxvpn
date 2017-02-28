{ stdenv, clangStdenv, lib, fetchFromGitHub               # Nix-related
, cmake                                                   # Build tools
, libsodium, systemd, jsoncpp, libtoxcore, libcap, zeromq # Libraries
, enableDebugging ? false
}:

with {
  libtoxcoreLocked = stdenv.lib.overrideDerivation libtoxcore (old: {
    name = "libtoxcore-20160907";

    src = fetchFromGitHub {
      owner  = "TokTok";
      repo   = "c-toxcore";
      rev    = "1387c8f15032cab1af1c6444621b62af8d3a5494";
      sha256 = "1wd5l56fb1lrrjxsgnzr6199kiw81jipyr8f395gbka4bzysilzq";
    };

    dontStrip = enableDebugging;
  });
};

stdenv.mkDerivation {
  name = "toxvpn-git";

  src = ./.;

  dontStrip = enableDebugging;

  NIX_CFLAGS_COMPILE = if enableDebugging then [ "-ggdb -Og" ] else [];

  buildInputs = lib.concatLists [
    [ cmake libtoxcoreLocked jsoncpp libsodium libcap zeromq ]
    (lib.optional (systemd != null) systemd)
  ];

  cmakeFlags = lib.concatLists [
    (lib.optional (systemd != null) [ "-DSYSTEMD=1" ])
    [ "-DBOOTSTRAP_PATH=$\{out}/share/toxvpn/bootstrap.json" ]
  ];

  postInstall = ''
      mkdir -pv ''${out}/share/toxvpn
      cp -vi ../bootstrap.json ''${out}/share/toxvpn/bootstrap.json
  '';

  meta = with stdenv.lib; {
    description = "A powerful tool that allows one to make tunneled point to point connections over Tox";
    homepage    = "https://github.com/cleverca22/toxvpn";
    license     = licenses.gpl3;
    maintainers = with maintainers; [ cleverca22 obadz ];
    platforms   = platforms.linux;
  };
}
