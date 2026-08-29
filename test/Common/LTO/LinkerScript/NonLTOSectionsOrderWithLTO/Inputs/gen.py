import sys

N = 300
output = sys.argv[1]

with open(output, 'w') as f:
    f.write("int bar();\n")
    f.write("int baz();\n")
    for i in range(N):
        f.write(f"int foo_{i}() {{ return {i}; }}\n")
    f.write("int main() {\n  return ")
    for i in range(N):
        f.write(f"foo_{i}()")
        if i != N - 1:
            f.write(" + ")
    f.write(" + baz() + bar();\n}\n")
