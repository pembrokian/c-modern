import helper_box

func main() i32 {
    boxed := helper_box.Box<i32>{ value: 7 }
    return helper_box.read_i32(boxed)
}