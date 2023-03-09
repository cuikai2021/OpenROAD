#!/usr/bin/env python3
import os
import sys
import yaml
import shutil
import hashlib
from .utils import system_run, docker_build

class ConfigError(Exception):
    def __init__(self, messages):
        self.messages = messages
        super().__init__(self.messages)

class Tool(object):
    def __init__(
            self,
            name,
            repo,
            commit,
            build_script,
            file,
    ):
        self.name = name
        self.repo = repo
        self.long_commit = commit
        if 'file://' in repo:
            path = repo.replace("file://", "")
            self.long_commit = system_run(f'git -C {path} rev-parse HEAD 2>/dev/null', output = True).replace('\n', '')
            self.commit = self.long_commit[:7]
            diff_summary = system_run(f'git -C {path} diff', output = True)
            if diff_summary != '':
                diff_hash = hashlib.sha1(diff_summary.encode('utf-8')).hexdigest()
                self.commit += f'-modified-{diff_hash[:7]}'
        elif 'docker://' in repo:
            self.commit = commit
        else:
            if commit == '' :
                raise ConfigError(f'{name}.repo is "https://" type require commit')
            self.commit = commit[:7]
        self.build_script = build_script
        self.file = file

    def checkout(self, dst=None):

        if dst is not None:
            system_run(f"rm -rf {dst}")
        else:
            system_run(f'rm -rf /tmp/{self.name}')
            dst = f'/tmp/{self.name}'
        src = self.repo
        if 'file://' in src:
            src = self.repo.replace('file://', '')
            dst = src
        elif 'oss://' in src:
            src += f'/{self.commit}'
            system_run(f'ossutil64 cp -r {src}/ {dst}')
        else:
            if shutil.which('checkout.sh') is None:
                system_run(f'git clone --recurse-submodules {src} {dst}')
                system_run(f'git -C {dst} checkout {self.commit} 2>/dev/null')
            else:
                system_run(f'checkout.sh \'{self.repo.replace("https://github.com/", "")} --dir {dst} --ref {self.commit}\'')

        return dst

    def build(self, **kwargs):

        if 'docker://' in self.repo:
            raise ConfigError(f'"docker://" type repo can\'t build')

        tool_path = self.checkout()
        docker_file_string=''
        has_build = False

        if self.build_script is not None:
            docker_file_string += f'FROM {kwargs["base_image"]} AS builder\n'
            docker_file_string += f'COPY . /{self.name}\n'
            docker_file_string += f'ENV REPO_COMMIT={self.long_commit}\n'
            if kwargs.get('build_type'):
                docker_file_string += f'ENV BUILD_TYPE={kwargs["build_type"]}\n'
            docker_file_string += f'RUN cd /{self.name} && {self.build_script}\n'
            has_build = True

        docker_file_string += 'FROM registry.cn-shanghai.aliyuncs.com/icb/alpine:latest\n'

        for file in self.file.split('\n'):
            if file == '':
                break
            src = file.split(':')[0]
            dst = file.split(':')[1]
            try:
                mod = file.split(':')[2]
            except IndexError:
                mod = ''
            copy_string = 'COPY'
            if mod != '':
                copy_string += f' --chmod={mod}'
            if has_build == True:
                copy_string += f' --from=builder /{self.name}/{src}'
            else:
                copy_string += f' {src}'
            copy_string += f' /build-tmp{dst}\n'
            docker_file_string += copy_string

        yaml_string = self.to_metadata_yaml()
        yaml_string = yaml_string.replace('\n', '\\n')
        docker_file_string += f'RUN mkdir -p /build-tmp/build/version && echo -e "{yaml_string}" > /build-tmp/build/version/{self.name}_metadata.yaml\n'
        docker_file_string += f'RUN mkdir -p /build\n'

        system_run('mkdir -p /tmp/Dockerfile')
        with open(f'/tmp/Dockerfile/Dockerfile.{self.name}', 'w') as f:
            f.write(docker_file_string)

        docker_build(
            docker_file = f'/tmp/Dockerfile/Dockerfile.{self.name}',
            context = tool_path,
            **kwargs
        )

    def get_docker_id(self, prefix) -> str:
        if 'docker://' in self.repo:
            return self.repo.replace('docker://', '') + ':' + self.commit
        else:
            return f'{prefix}:{self.name}-{self.commit}'

    def to_metadata_yaml(self) -> str:
        yaml_dict = {}
        yaml_dict['name'] = self.name
        yaml_dict['repo'] = self.repo
        yaml_dict['commit'] = self.commit
        yaml_dict['file'] = self.file
        return yaml.dump(yaml_dict)

    @staticmethod
    def from_metadata_yaml(metadata_yaml: str) -> list:
        tools = []
        tool_list = yaml.load(metadata_yaml, Loader=yaml.SafeLoader)
        for tool in tool_list:
            tools.append(Tool(
                name=tool['name'],
                repo=tool.get('repo'),
                commit=tool.get('commit') or '',
                build_script=tool.get('build') or None,
                file=tool.get('file') or '',
            )
            )
        return tools

with open('./dependencies/tools_metadata.yaml', 'r') as f:
    metadata = f.read()
Tools = Tool.from_metadata_yaml(metadata)

def tools_by_name() -> list:
    tools_list = []
    for tool in Tools:
        tools_list.append(tool.name)
    return tools_list

def main():
    for tool in tools_by_name():
        print(tool)

if __name__ == '__main__':
    main()