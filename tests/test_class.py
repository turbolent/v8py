import pytest
import v8py


@pytest.fixture()
def Test():
    class Test:
        def method(self):
            return 'thing'

        @staticmethod
        def static_method():
            return 'static'

        @classmethod
        def class_method(cls):
            return 'class'

        @v8py.hidden
        def hidden_method(self):
            return 'hidden'

    return Test


@pytest.fixture
def context(Test, context):
    context.glob.Test = Test
    return context


def test_call_method(context):
    assert context.eval('new Test().method()') == 'thing'


def test_call_method_on_wrong_object(context):
    with pytest.raises(Exception):
        context.eval('new Test().method.call({})')


def test_class_name(context):
    assert context.eval('Test.name') == 'Test'
    assert context.eval('new Test().toString()') == '[object Test]'


def test_calling_methods_from_python(context):
    context.eval('test = new Test()')
    assert context.eval('test.method()') == 'thing'


def test_object_passthrough(context):
    test = context.eval('test = new Test()')
    assert test is context.test


def test_static_method(context):
    assert context.eval('Test.static_method()') == 'static'
    assert context.eval('Test.class_method()') == 'class'


def test_hidden_method(context):
    with pytest.raises(v8py.JSException):
        context.eval('new Test().hidden_method()')
