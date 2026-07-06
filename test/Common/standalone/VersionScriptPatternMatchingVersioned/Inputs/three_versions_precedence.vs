V1 {
    global:
        *;
};

V2 {
    global:
        foo*;
} V1;

V3 {
    global:
        foo1;
    local:
        *;
} V2;
