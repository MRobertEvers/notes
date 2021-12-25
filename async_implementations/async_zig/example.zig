var global_var: u8 = 5;
var frame: @Frame(func) = undefined;
var a: u8 = 5;
var blarg: u16 = 0;
fn amain() void {
    frame = async func(&a, 4, 5);

    resume &frame;
    blarg = await &frame;

    a += @intCast(u8, blarg);
}

fn func(me: *u8, s: u8, b: u8) u16 {
    var local_var: u8 = 127;
    me.* += 6;
    suspend {}
    me.* += 2;
    suspend {}
    me.* += s + b + local_var;
    suspend {}
    me.* += s + b + local_var;
    suspend {}
    me.* += s + b + local_var;
    suspend {}
    return @intCast(u16, me.*);
}

export fn main() void {
    _ = async amain();
}