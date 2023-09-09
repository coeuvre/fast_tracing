#include "src/json.h"

#include <gtest/gtest.h>

struct TestJsonInputContext {
    const char **inputs;
    int input_count;
    int input_index;
};

static bool test_json_input_fetch(void *ctx_, MemoryArena *arena, Buf *buf,
                                  JsonError *error) {
    TestJsonInputContext *ctx = (TestJsonInputContext *)ctx_;
    if (ctx->input_index >= ctx->input_count) {
        return false;
    }

    const char *input = ctx->inputs[ctx->input_index++];
    *buf = {
        .data = (u8 *)input,
        .size = strlen(input),
    };
    return true;
}

static void run_json_scan_test(const char *inputs[], int input_count,
                               JsonToken tokens[], int token_count,
                               JsonError expected_error) {
    TestJsonInputContext ctx = {
        .inputs = inputs,
        .input_count = input_count,
        .input_index = 0,
    };
    JsonInput input;
    json_input_init(&input, &ctx, test_json_input_fetch);

    MemoryArena arena;
    memory_arena_init(&arena);

    int token_index = 0;
    JsonToken token;
    JsonError error;
    while (json_scan(&arena, &input, &token, &error)) {
        ASSERT_LT(token_index, token_count);
        JsonToken *expected_token = &tokens[token_index++];

        if (token.value.data) {
            char *value = strndup((char *)token.value.data, token.value.size);
            ASSERT_STREQ(value, (char *)expected_token->value.data);
            free(value);
        } else {
            ASSERT_EQ(token.value.data, expected_token->value.data);
        }

        ASSERT_EQ(token.value.size, expected_token->value.size);
        ASSERT_EQ(token.type, expected_token->type);
    }

    ASSERT_EQ(token_index, token_count)
        << "The number of generated Tokens is less than expected";

    ASSERT_EQ(error.has_error, expected_error.has_error);
    ASSERT_STREQ((char *)error.message.data,
                 (char *)expected_error.message.data);
    ASSERT_EQ(error.message.size, expected_error.message.size);

    memory_arena_deinit(&arena);
}

TEST(JsonScanTest, String) {
    const char *inputs[] = {
        " \"a",
        "b",
        "\" ",
    };
    JsonToken tokens[] = {
        {.type = JsonToken_String, .value = STR_LITERAL("ab")},
    };
    run_json_scan_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens),
                       {});
}

TEST(JsonScanTest, StringEscape) {
    const char *inputs[] = {
        " \"a",
        "b\\",
        "\\\" ",
    };
    JsonToken tokens[] = {
        {.type = JsonToken_String, .value = STR_LITERAL("ab\\\\")},
    };
    run_json_scan_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens),
                       {});
}

TEST(JsonScanTest, StringEscapeU) {
    const char *inputs[] = {
        "\"\\uabcd\"",
    };
    JsonToken tokens[] = {
        {.type = JsonToken_String, .value = STR_LITERAL("\\uabcd")},
    };
    run_json_scan_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens),
                       {});
}

TEST(JsonScanTest, StringEof) {
    const char *inputs[] = {
        " \"a",
        " ",
    };
    JsonToken tokens[] = {};
    run_json_scan_test(
        inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens),
        {.has_error = true,
         .message = STR_LITERAL(
             "End of string '\"' expected but reached end of input")});
}

TEST(JsonScanTest, Integer) {
    const char *inputs[] = {
        " 1",
        "2",
        "3 ",
    };
    JsonToken tokens[] = {
        {.type = JsonToken_Number, .value = STR_LITERAL("123")},
    };
    run_json_scan_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens),
                       {});
}

TEST(JsonScanTest, Fraction) {
    const char *inputs[] = {
        " 1.",
        "2",
        "3 ",
    };
    JsonToken tokens[] = {
        {.type = JsonToken_Number, .value = STR_LITERAL("1.23")},
    };
    run_json_scan_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens),
                       {});
}

TEST(JsonScanTest, Exponent) {
    const char *inputs[] = {" 1e", "2", "3 "};
    JsonToken tokens[] = {
        {.type = JsonToken_Number, .value = STR_LITERAL("1e23")},
    };
    run_json_scan_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens),
                       {});
}

TEST(JsonScanTest, ObjectBeginEnd) {
    const char *inputs[] = {
        "{",
        "}",
    };
    JsonToken tokens[] = {
        {.type = JsonToken_ObjectStart},
        {.type = JsonToken_ObjectEnd},
    };
    run_json_scan_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens),
                       {});
}

TEST(JsonScanTest, ArrayBeginEnd) {
    const char *inputs[] = {
        " [",
        "] ",
    };
    JsonToken tokens[] = {
        {.type = JsonToken_ArrayStart},
        {.type = JsonToken_ArrayEnd},
    };
    run_json_scan_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens),
                       {});
}

TEST(JsonScanTest, Colon) {
    const char *inputs[] = {
        " ",
        ": ",
    };
    JsonToken tokens[] = {
        {.type = JsonToken_Colon},
    };
    run_json_scan_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens),
                       {});
}

TEST(JsonScanTest, Comma) {
    const char *inputs[] = {" ", ", "};
    JsonToken tokens[] = {
        {.type = JsonToken_Comma},
    };
    run_json_scan_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens),
                       {});
}

TEST(JsonScanTest, True) {
    const char *inputs[] = {"  t", "ru", "e "};
    JsonToken tokens[] = {
        {.type = JsonToken_True},
    };
    run_json_scan_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens),
                       {});
}

TEST(JsonScanTest, False) {
    const char *inputs[] = {"  f", "al", "s", "e "};
    JsonToken tokens[] = {
        {.type = JsonToken_False},
    };
    run_json_scan_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens),
                       {});
}

TEST(JsonScanTest, Null) {
    const char *inputs[] = {"  n", "ul", "l "};
    JsonToken tokens[] = {
        {.type = JsonToken_Null},
    };
    run_json_scan_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens),
                       {});
}