int dynadd(int a, int b);
int dynlen(const char *s);

int main(void) {
    return dynadd(40, 2) + dynlen("hello");
}
