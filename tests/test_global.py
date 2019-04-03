import pytest

from v8py import Context


@pytest.fixture()
def Global():
    class Global:
        def method(self):
            return 'method'

        @property
        def property(self):
            return 'property'

    return Global


@pytest.fixture
def context(Global):
    return Context(Global)


def test_global(context):
    assert context.eval('method()') == 'method'
    assert context.eval('this.method()') == 'method'

    assert context.eval('property') == 'property'
    assert context.eval('this.property') == 'property'
