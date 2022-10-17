# How to add a dependency tool

| fields | description | required |
| ------ | ------ | ------ |
|  name  | tool name | yes |
|  repo  | url to the git or docker repository. `https://`, `file://` or `docker://` | yes |
|  file  | the files should be copy to final release docker image, `src:dst[:permissions]` | yes |
| commit | git commit id or docker tag | yes |
|  build | how to build this tool, bash syntax. if there is no `build` filed, you should build locally first | no |

# How to build faster

You can build locally, then change the type of `repo://` to `file://` and remove `build` field
