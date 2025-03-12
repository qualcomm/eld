import lit.formats
import lit.Test


class ThinArchiveTestFormat(lit.formats.ShTest):
    def execute(self, test, litConfig):
        with open(test.getSourcePath()) as f:
            if "%ar " not in f.read():
                return lit.Test.Result(lit.Test.SKIPPED, "Test is skipped")
        return super().execute(test, litConfig)
