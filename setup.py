# Compile manually
# c++ -O3 -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -shared -std=c++11 -fPIC
# `python3 -m pybind11 --includes`
# -I/home/david/cpp/pgm_dataset/venv/lib/python3.6/site-packages/pyarrow/include cpp/data.cpp
# -I/home/david/cpp/pgm_dataset/venv/lib/python3.6/site-packages/numpy/core/include
# -L/home/david/cpp/pgm_dataset/venv/lib/python3.6/site-packages/pyarrow/
# -larrow
# -larrow_python
# -Wl,-rpath,$RPATH -o data`python3-config --extension-suffix`
# Set LD flag
# LD_LIBRARY_PATH=/home/david/cpp/pgm_dataset/venv/lib/python3.6/site-packages/pyarrow
# export LD_LIBRARY_PATH

from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
from distutils.command import build as build_module
import sys
import setuptools
import os

from numpy.distutils.conv_template import process_file as process_c_file

__version__ = '0.0.1'

# os.environ['CC'] = "ccache gcc-10"
os.environ['CC'] = "ccache clang-10"

class get_pybind_include(object):
    """Helper class to determine the pybind11 include path

    The purpose of this class is to postpone importing pybind11
    until it is actually installed, so that the ``get_include()``
    method can be invoked. """

    def __init__(self, user=False):
        self.user = user

    def __str__(self):
        import pybind11
        print("Include pybind11: " + pybind11.get_include(self.user))
        return pybind11.get_include(self.user)

import pyarrow as pa

# https://stackoverflow.com/questions/49266003/setuptools-build-shared-libary-from-c-code-then-build-cython-wrapper-linked
ext_lib_path = 'lib/libfort'
sources = ['fort.c']
ext_libraries = [['fort', {
               'sources': [os.path.join(ext_lib_path, src) for src in sources],
               'include_dirs': [ext_lib_path],
               'extra_compile_args': ['-D_GLIBCXX_USE_CXX11_ABI=0']
               }
]]

# Ignore warnings from this files.
system_headers = ['-isystem' + d for d in [pa.get_include()]] +\
                 ["-isystemlib/eigen-3.3.7"] +\
                 ['-isystemlib/OpenCL']

ext_modules = [
    Extension(
        'pgm_dataset',
        [
         'src/lib.cpp',
         'src/pybindings/pybindings_dataset.cpp',
         'src/pybindings/pybindings_factors.cpp',
         'src/pybindings/pybindings_models.cpp',
         'src/pybindings/pybindings_learning/pybindings_learning.cpp',
         'src/pybindings/pybindings_learning/pybindings_scores.cpp',
         'src/pybindings/pybindings_learning/pybindings_parameters.cpp',
         'src/pybindings/pybindings_learning/pybindings_mle.cpp',
         'src/pybindings/pybindings_learning/pybindings_operators.cpp',
         'src/pybindings/pybindings_learning/pybindings_algorithms.cpp',
         'src/factors/continuous/LinearGaussianCPD.cpp',
         'src/factors/continuous/CKDE.cpp',
         'src/factors/continuous/SemiparametricCPD.cpp',
         'src/factors/discrete/DiscreteFactor.cpp',
         'src/dataset/dataset.cpp',
         'src/dataset/crossvalidation_adaptator.cpp',
         'src/dataset/holdout_adaptator.cpp',
         'src/util/bit_util.cpp',
         'src/util/validate_dtype.cpp',
         'src/learning/operators/operators.cpp',
         'src/learning/algorithms/hillclimbing.cpp',
         'src/learning/algorithms/pc.cpp',
         'src/learning/independence_tests/continuous/linearcorrelation.cpp',
         'src/graph/dag.cpp',
         'src/graph/undirected.cpp',
         'src/models/BayesianNetwork.cpp',
         'src/opencl/opencl_config.cpp',
         ],
        include_dirs=[
        #     # Path to pybind11 headers
            get_pybind_include(),
            get_pybind_include(user=True),
            "src",
            "lib/boost",
            "lib/libfort",
        ],
        libraries=pa.get_libraries() + ["OpenCL"] + ["fort"],
        library_dirs=pa.get_library_dirs(),
        language='c++',
        # Included as isystem to avoid errors in arrow headers.
        extra_compile_args=system_headers,
    ),
]

# As of Python 3.6, CCompiler has a `has_flag` method.
# cf http://bugs.python.org/issue26689
def has_flag(compiler, flagname):
    """Return a boolean indicating whether a flag name is supported on
    the specified compiler.
    """
    import tempfile
    with tempfile.NamedTemporaryFile('w', suffix='.cpp') as f:
        f.write('int main (int argc, char **argv) { return 0; }')
        try:
            compiler.compile([f.name], extra_postargs=[flagname])
        except setuptools.distutils.errors.CompileError:
            return False
    return True


def cpp_flag(compiler):
    """Return the -std=c++[11/14/17] compiler flag.

    The newer version is prefered over c++11 (when it is available).
    """
    flags = ['-std=c++17']

    for flag in flags:
        if has_flag(compiler, flag): return flag

    raise RuntimeError('Unsupported compiler -- at least C++17 support '
                       'is needed!')


def expand_sources():
    sources = ['src/factors/continuous/opencl/CKDE.cl.src']

    for source in sources:
        (base, _) = os.path.splitext(source)
        outstr = process_c_file(source)
        with open(base, 'w') as fid:
            fid.write(outstr)

def create_symlinks():
    pa.create_library_symlinks()


class BuildExt(build_ext):
    """A custom build extension for adding compiler-specific options."""
    c_opts = {
        'msvc': ['/EHsc'],
        'unix': [],
    }
    l_opts = {
        'msvc': [],
        'unix': [],
    }

    if sys.platform == 'darwin':
        darwin_opts = ['-stdlib=libc++', '-mmacosx-version-min=10.7']
        c_opts['unix'] += darwin_opts
        l_opts['unix'] += darwin_opts

    def build_extensions(self):
        expand_sources()
        create_symlinks()

        ct = self.compiler.compiler_type
        opts = self.c_opts.get(ct, [])
        link_opts = self.l_opts.get(ct, [])
        # Include this because the name mangling affects to find the pyarrow functions.
        opts.append("-D_GLIBCXX_USE_CXX11_ABI=0")
        # opts.append("-ferror-limit=1")

        opts.append("-Wall")
        opts.append("-Wextra")
        # opts.append("-fsyntax-only")
        opts.append("-march=native")
        opts.append("-fdiagnostics-color=always")

        # This reduces the binary size because it removes the debug symbols. Check strip command to create release builds.
        # opts.append("-g0")
        if ct == 'unix':
            opts.append('-DVERSION_INFO="%s"' % self.distribution.get_version())
            opts.append(cpp_flag(self.compiler))
            if has_flag(self.compiler, '-fvisibility=hidden'):
                opts.append('-fvisibility=hidden')
        elif ct == 'msvc':
            opts.append('/DVERSION_INFO=\\"%s\\"' % self.distribution.get_version())
        for ext in self.extensions:
            ext.extra_compile_args.extend(opts)
            ext.extra_link_args.extend(link_opts)
        build_ext.build_extensions(self)

setup(
    name='pgm_dataset',
    version=__version__,
    author='David Atienza',
    author_email='datienza@fi.upm.es',
    # url='https://github.com/pybind/python_example',
    description='A test project using pybind11',
    long_description='',
    ext_modules=ext_modules,
    libraries=ext_libraries,
    setup_requires=['pybind11>=2.4', 'pyarrow'],
    install_requires=['pybind11>=2.4', 'pyarrow'],
    cmdclass={'build_ext': BuildExt},
    zip_safe=False,
)
