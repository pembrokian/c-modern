import expr
import testing

func test_token_count() *i32 {
    if expr.token_count("1 + (2 + 3)") != 7 {
        return testing.fail()
    }
    return nil
}