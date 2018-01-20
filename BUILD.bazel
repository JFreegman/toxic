cc_binary(
    name = "toxic",
    srcs = glob([
        "src/*.c",
        "src/*.h",
    ]),
    copts = [
        "-DAUDIO",
        "-DPACKAGE_DATADIR='\"data\"'",
        "-DPYTHON",
        "-DVIDEO",
    ],
    linkopts = [
        "-lconfig",
        "-lncurses",
        "-lopenal",
        "-lX11",
    ],
    deps = [
        "//c-toxcore",
        "@curl",
        "@libqrencode",
        "@libvpx",
        "@python3//:python",
    ],
)
