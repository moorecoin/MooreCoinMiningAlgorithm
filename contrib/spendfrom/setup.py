from distutils.core import setup
setup(name='btcspendfrom',
      version='1.0',
      description='command-line utility for moorecoin "coin control"',
      author='gavin andresen',
      author_email='gavin@moorecoinfoundation.org',
      requires=['jsonrpc'],
      scripts=['spendfrom.py'],
      )
