import sys
import hashlib
import subprocess

DOCKER_REGISTRY = 'registry.cn-shanghai.aliyuncs.com'

def system_run(command, output = False):
    try:
        if output == True:
            result = subprocess.run(command, shell = True, check = True, stdout = subprocess.PIPE)
            return result.stdout.decode('utf-8')
        else:
            subprocess.run(command, shell = True, check = True)
            return None
    except subprocess.CalledProcessError as e:
        raise e

def test_manifest_exists(docker_id) -> bool:
    try:
        system_run(f'(docker inspect {docker_id} || docker manifest inspect {docker_id}) >/dev/null 2>&1')
        return True
    except subprocess.CalledProcessError:
        return False

def docker_build(**kwargs):
    docker_id = kwargs.get('docker_id') \
                or f'{DOCKER_REGISTRY}/{kwargs["docker_repo"]}/{kwargs["docker_name"]}:{kwargs["docker_tag"]}'

    build_args_string = ''
    for key in kwargs.get('build_args') or {}:
        build_args_string += f'--build-arg {key}={kwargs["build_args"][key]}'

    build_option = ''
    if kwargs['progress_plain'] == True:
        build_option += '--progress=plain'

    target_string = ''
    if kwargs.get('build_target'):
        target_string += f'--target {kwargs["build_target"]}'

    system_run(
        f'DOCKER_BUILDKIT=1 docker build {build_option} \
        -t {docker_id} \
        -f {kwargs["docker_file"]} \
        {target_string} \
        {build_args_string} \
        {kwargs["context"]}'
    )

    if kwargs['push']:
        system_run(f'DOCKER_BUILDKIT=1 docker push {docker_id}')

def get_sha1sum(files) -> str:
    files.sort()
    sha1 = hashlib.sha1()

    # 64kb chunks
    BUF_SIZE = 65536
    for file in files:
        with open(file, 'rb') as f:
            while True:
                data = f.read(BUF_SIZE)
                if not data:
                    break
                sha1.update(data)

    return sha1.hexdigest()