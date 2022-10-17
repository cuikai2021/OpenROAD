# Install python dependencies
`pip3 install -r requirements.txt`

# How to run build script

```
$ python3 build.py --help

Usage: build.py [OPTIONS] COMMAND [ARGS]...

Options:
  -b, --base-image TEXT
  -r, --docker-repo [icb|icbench-dev]
                                  [default: icb]
  -t, --docker-tag TEXT
  -p, --push                      [default: False]
  --progress-plain                buildkit option, show docker build output
                                  [default: False]
  ...
```

Here are some options that do not have unified defaults

| type | docker-tag | base-image |
| ---- | ---------- | ---------- |
| build-deps | `sha1sum of all "*.so" files` | registry.cn-shanghai.aliyuncs.com/icb/eden_deps_ubuntu:latest |
| release | latest | registry.cn-shanghai.aliyuncs.com/icb/eden_build_ubuntu:latest |

## release
options:
  - `-i/--ignore-exist-manifest`

  if you change field other than "name" or "commit" in `tools_metadata.yaml`, adding this option will ignore the exists docker image, script will build new docker image.

  - `--debug`

  the default build is the release image, add this option to build the debug enabled image

example:
```
python3 build.py release
python3 build.py -t 220925 release -i eden -i maple
python3 build.py -t 220926 release --debug
```

## build_deps

example:
```
python3 build.py build-deps
```
