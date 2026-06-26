import lit.Test
import lit.formats


class ThinArchiveTestFormat(lit.formats.TestFormat):
    def __init__(self, execute_external, base_format=None):
        self.base_format = base_format or lit.formats.ShTest(execute_external)

    def getTestsInDirectory(self, testSuite, path_in_suite, litConfig,
                            localConfig):
        return self.base_format.getTestsInDirectory(
            testSuite, path_in_suite, litConfig, localConfig)

    def execute(self, test, litConfig):
        with open(test.getSourcePath()) as f:
            if "%ar " not in f.read():
                return lit.Test.Result(lit.Test.SKIPPED, "Test is skipped")
        return self.base_format.execute(test, litConfig)
