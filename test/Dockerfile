FROM alpine:3.19.0

RUN ["apk", "add", "--no-cache", \
 "bash", \
 "cmake", \
 "curl-dev", \
 "g++", \
 "gcc", \
 "git", \
 "libconfig-dev", \
 "libsodium-dev", \
 "libvpx-dev", \
 "linux-headers", \
 "make", \
 "ncurses-dev", \
 "opus-dev", \
 "pkgconfig", \
 "samurai", \
 "yarn"]

WORKDIR /build
RUN yarn add --dev @microsoft/tui-test
ENV PATH=$PATH:/build/node_modules/.bin

WORKDIR /build
RUN git clone --depth=1 --recursive https://github.com/TokTok/c-toxcore /build/c-toxcore \
 && cmake -GNinja -B/build/c-toxcore/_build -H/build/c-toxcore \
 -DBOOTSTRAP_DAEMON=OFF \
 -DENABLE_STATIC=OFF \
 -DMUST_BUILD_TOXAV=ON \
 && cmake --build /build/c-toxcore/_build --target install

WORKDIR /build/toxic
COPY Makefile /build/toxic/
COPY cfg /build/toxic/cfg/
COPY misc /build/toxic/misc/
COPY sounds /build/toxic/sounds/
COPY src /build/toxic/src/
ENV CFLAGS="-D_GNU_SOURCE -Werror"
RUN make "-j$(nproc)" install

WORKDIR /build/toxic/test
ENTRYPOINT ["tui-test"]
