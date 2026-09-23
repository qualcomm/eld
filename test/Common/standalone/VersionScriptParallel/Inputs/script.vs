V1 {
    global:
        foo;
        bar*;
    local:
        internal_*;
};

V2 {
    global:
        helper*;
    local:
        *;
} V1;
