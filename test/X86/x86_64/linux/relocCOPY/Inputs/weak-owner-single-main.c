extern int weak_obj;

int main(void) {
    return weak_obj == 42 ? 0 : 1;
}
