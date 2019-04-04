#!/usr/bin/env python
from __future__ import print_function

import sys
import os
import stat
from contextlib import contextmanager
from subprocess import check_call
import zipfile
import multiprocessing

from urllib.request import urlretrieve

from setuptools import setup, find_packages, Extension, Command
from distutils.command.build_ext import build_ext as distutils_build_ext

os.chdir(os.path.abspath(os.path.dirname(__file__)))

sources = list(map(lambda path: os.path.join('v8py', path),
                   filter(lambda path: path.endswith('.cpp'),
                          os.listdir('v8py'))))

v8_libraries = ["v8", "v8_libbase", "v8_libplatform", "icui18n", "icuuc"]

libraries = list(v8_libraries)

library_dirs = ["/usr/local/opt/v8/libexec", "v8/out.gn"]
include_dirs = ["/usr/local/opt/v8/libexec/include", "v8/include"]
extra_compile_args = ['-std=c++11']
extra_link_args = []

if sys.platform.startswith('linux'):
    libraries.append('rt')
    libraries.append('stdc++')

if sys.platform.startswith('darwin'):
    extra_compile_args.append('-stdlib=libc++')

extension = Extension('_v8py',
                      sources=sources,
                      include_dirs=include_dirs,
                      library_dirs=library_dirs,
                      libraries=libraries,
                      extra_compile_args=extra_compile_args,
                      extra_link_args=extra_link_args,
                      language='c++')

DEPOT_TOOLS_PATH = os.path.join(os.getcwd(), 'depot_tools')
COMMAND_ENV = os.environ.copy()
COMMAND_ENV['PATH'] = DEPOT_TOOLS_PATH + os.path.pathsep + os.environ['PATH']

COMMAND_ENV.pop('CC', None)
COMMAND_ENV.pop('CXX', None)


def run(command):
    print(command)
    check_call(command, shell=True, env=COMMAND_ENV)


with open('README.rst', 'r') as f:
    long_description = f.read()

setup(
    name='v8py',
    version='0.9.15',

    author='Theodore Dubois',
    author_email='tblodt@icloud.com',
    url='https://github.com/tbodt/v8py',

    description='Write Python APIs, then call them from JavaScript using the V8 engine.',
    long_description=long_description,

    license='LGPLv3',
    classifiers=[
        'Development Status :: 3 - Alpha',
        'License :: OSI Approved :: GNU Lesser General Public License v3 or later (LGPLv3+)',
        'Topic :: Software Development :: Interpreters',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3.5',
    ],

    keywords=['v8', 'javascript'],

    packages=find_packages(exclude=['tests']),
    ext_modules=[extension],

    extras_require={
        'devtools': ['gevent', 'karellen-geventws'],
    },
    setup_requires=['pytest-runner'],
    tests_require=['pytest']
)
