import sys
import pytest
import types


@pytest.fixture()
def Test(request):
    class Test:
        value = 'value'

        @property
        def prop(self):
            return self.value + ' property'

        @prop.setter
        def prop(self, value):
            self.value = value

        @property
        def unsettable(self):
            return 'unsettable'

        def __getitem__(self, key):
            if key == 'getitem':
                return self.value

        def __setitem__(self, key, value):
            if key == 'getitem':
                self.value = value

        def __delitem__(self, key):
            if key == 'getitem':
                self.value = None

        def keys(self):
            return ['getitem']

    return Test


@pytest.fixture
def context(Test, context):
    context.Test = Test
    context.test = Test()
    return context


def test_getitem(context):
    assert context.eval('test.getitem') == 'value'


def test_setitem(context):
    context.eval('test.getitem = "harambe"')
    # proof harambe is still alive
    assert context.eval('test.getitem') == 'harambe'


def test_delitem(context):
    assert context.eval('test.getitem') == 'value'
    context.eval('delete test.getitem')
    assert context.eval('test.getitem') == None


def test_query(context):
    descriptor = context.eval('Object.getOwnPropertyDescriptor(test, "getitem")')
    assert descriptor['writable']
    assert not descriptor['enumerable']
    assert descriptor['configurable']
    assert descriptor['value'] == 'value'


# TODO:
# def test_enumerate(context, Test):
#     name_list = context.eval('Object.getOwnPropertyNames(test)')
#     assert 'getitem' in name_list
#     if isinstance(Test, type):
#         assert len(name_list) == 3
#         assert 'prop' in name_list
#     else:
#         assert len(name_list) == 1

# data descriptors aren't available on old-style classes
def test_get_property(context, Test):
    if isinstance(Test, type):
        assert context.eval('test.prop') == 'value property'


def test_set_property(context, Test):
    if isinstance(Test, type):
        context.eval('test.prop = "kappa"')
        assert context.eval('test.prop') == 'kappa property'


def test_unsettable(context, Test):
    if isinstance(Test, type):
        assert not context.eval('Object.getOwnPropertyDescriptor(test, "unsettable").writable')
        context.eval('test.unsettable = "kappa"')
