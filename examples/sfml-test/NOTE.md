To run grafon-edition applications in docker container:

```bash
# Allow connection to the X server on the host
xhost +local:root

docker run --cap-add=SYS_PTRACE      \
--security-opt seccomp=unconfined    \
--rm -it --network=host              \
-e DISPLAY=$DISPLAY                  \
-v ${WorkDir}:/mnt/yandex-cpp-middle \
-v /tmp/.X11-unix:/tmp/.X11-unix     \
${DockerImageId}
```

You need to bypass X11 authorization (yes, such thing exists) by using `xhost` to allow root user to use your X server (it's not a good practice, you should forward your personal magic cookie inside the docker container, but fuck it), set `-e DISPLAY=$DISPLAY` and mount X11 shit via `-v /tmp/.X11-unix`.
It should work by default, but you can check the exact name in /tmp folder just in case.
