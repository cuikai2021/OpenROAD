import os
import sys
import click
from pathlib import Path

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
CURRENT_DIR = os.getcwd()
os.chdir(ROOT_DIR)
sys.path.append(ROOT_DIR)
from dependencies import tool, utils

@click.group()
@click.option('-b', '--base-image')
@click.option('-r', '--docker-repo', type=click.Choice(['icb', 'icbench-dev']), default='icb', show_default=True)
@click.option('-t', '--docker-tag')
@click.option('-p', '--push', is_flag=True, show_default=True)
@click.option('--progress-plain', is_flag=True, show_default=True, help='buildkit option, show docker build output')
@click.pass_context
def cli(ctx, **kwargs):
    ctx.obj = kwargs
    pass

tools_by_name = tool.tools_by_name()
tools_by_name.append("ALL")
@cli.command()
@click.option('-i', '--ignore-exist-manifest', type=click.Choice(tools_by_name), multiple=True, help='ignore exist manifest, build new docker image')
@click.option('--debug', 'build_type', flag_value='debug', help='build type')
@click.pass_context
def release(ctx, ignore_exist_manifest, build_type, **kwargs):
    kwargs.update(ctx.obj)
    if kwargs['base_image'] is None:
        kwargs['base_image'] = f'{utils.DOCKER_REGISTRY}/icb/eden_build_ubuntu:latest'
    if kwargs['docker_tag'] is None:
        kwargs['docker_tag'] = 'latest'
    if build_type is None:
        build_type = 'release'

    tools_docker_id = []
    tools_docker_repo = kwargs['docker_repo']
    tools_docker_name = 'eden_tools'

    if 'ALL' in ignore_exist_manifest:
        ignore_exist_manifest = tools_by_name

    from_lines = []
    copy_lines = []
    for i, t in enumerate(tool.Tools):
        tid = t.get_docker_id(f'{utils.DOCKER_REGISTRY}/{tools_docker_repo}/{tools_docker_name}')
        if t.name == 'eden' and build_type == 'debug':
            tid += '-debug'
        if t.name not in ignore_exist_manifest and utils.test_manifest_exists(tid):
            print(f'{tid} exists')
            tools_docker_id.append(tid)
        else:
            print(f'{tid} not exists or {t.name} in ignore_exist_manifest list')
            t.build(
                docker_id = tid,
                base_image = kwargs['base_image'],
                build_type = build_type,
                progress_plain = kwargs['progress_plain'],
                push = kwargs['push']
            )
            tools_docker_id.append(tid)
        from_lines.append(f'FROM {tid} as container{i}')
        if t.file != '':
            copy_lines.append(f'COPY --from=container{i} /build-tmp /')
        copy_lines.append(f'COPY --from=container{i} /build /build')

    with open('./docker/Dockerfile.release.tpl', 'r') as f:
        template = f.read()

    parts = template.split('# FROM')
    parts.insert(1, '\n'.join(from_lines))
    from_filled = ''.join(parts)
    parts = from_filled.split('# COPY')
    parts.insert(1, '\n'.join(copy_lines))
    final = ''.join(parts)

    utils.system_run('mkdir -p /tmp/Dockerfile')
    with open('/tmp/Dockerfile/Dockerfile.release', 'w') as f:
        f.write(final)

    utils.docker_build(
        docker_file = '/tmp/Dockerfile/Dockerfile.release',
        docker_name = f'eden_{build_type}_ubuntu',
        build_args = {
            'EDEN_BUILD_IMAGE': kwargs['base_image']
        },
        context = 'dependencies',
        **kwargs
    )


@cli.command()
@click.pass_context
def build_deps(ctx, **kwargs):
    kwargs.update(ctx.obj)
    if kwargs['base_image'] is None:
        kwargs['base_image'] = f'{utils.DOCKER_REGISTRY}/icb/eden_base_ubuntu:latest'
    if kwargs['docker_tag'] is None:
        files = []
        for file in Path(ROOT_DIR).rglob("*.so"):
            files.append(file.absolute())
        kwargs['docker_tag'] = utils.get_sha1sum(files)[:7]

    utils.docker_build(
        docker_file = 'docker/Dockerfile.build',
        docker_name = 'eden_build_ubuntu',
        build_args = {
            'eden_base_image': kwargs['base_image']
        },
        context = '.',
        **kwargs
    )


@cli.command()
@click.pass_context
def base(ctx, **kwargs):
    kwargs.update(ctx.obj)
    if kwargs['docker_tag'] is None:
        kwargs['docker_tag'] = 'latest'
    utils.docker_build(
        docker_file = 'docker/Dockerfile.base',
        docker_name = 'eden_base_ubuntu',
        build_target = 'base',
        context = '.',
        **kwargs
    )


cli()
os.chdir(CURRENT_DIR)
