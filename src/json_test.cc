#include "src/json.h"

#include <gtest/gtest.h>

#include "src/buf.h"

static void run_json_scan_test(Buf buf, JsonToken tokens[], int token_count,
                               JsonError expected_error) {
    JsonInput input;
    json_input_init(&input, buf);

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
    Buf input = STR_LITERAL(" \"ab\"");
    JsonToken tokens[] = {
        {.type = JsonToken_String, .value = STR_LITERAL("ab")},
    };
    run_json_scan_test(input, tokens, ARRAY_SIZE(tokens), {});
}

TEST(JsonScanTest, StringEscape) {
    Buf input = STR_LITERAL(" \"ab\\\\\" ");
    JsonToken tokens[] = {
        {.type = JsonToken_String, .value = STR_LITERAL("ab\\\\")},
    };
    run_json_scan_test(input, tokens, ARRAY_SIZE(tokens), {});
}

TEST(JsonScanTest, StringEscapeU) {
    Buf input = STR_LITERAL(" \"\\uabcd\"");
    JsonToken tokens[] = {
        {.type = JsonToken_String, .value = STR_LITERAL("\\uabcd")},
    };
    run_json_scan_test(input, tokens, ARRAY_SIZE(tokens), {});
}

TEST(JsonScanTest, StringEof) {
    Buf input = STR_LITERAL(" \"ab");
    JsonToken tokens[] = {};
    run_json_scan_test(
        input, tokens, ARRAY_SIZE(tokens),
        {.has_error = true,
         .message = STR_LITERAL(
             "End of string '\"' expected but reached end of input")});
}

TEST(JsonScanTest, Integer) {
    Buf input = STR_LITERAL(" 123 ");
    JsonToken tokens[] = {
        {.type = JsonToken_Number, .value = STR_LITERAL("123")},
    };
    run_json_scan_test(input, tokens, ARRAY_SIZE(tokens), {});
}

TEST(JsonScanTest, Integer2) {
    Buf input = STR_LITERAL(" 123");
    JsonToken tokens[] = {
        {.type = JsonToken_Number, .value = STR_LITERAL("123")},
    };
    run_json_scan_test(input, tokens, ARRAY_SIZE(tokens), {});
}

TEST(JsonScanTest, Fraction) {
    Buf input = STR_LITERAL(" 1.23 ");
    JsonToken tokens[] = {
        {.type = JsonToken_Number, .value = STR_LITERAL("1.23")},
    };
    run_json_scan_test(input, tokens, ARRAY_SIZE(tokens), {});
}

TEST(JsonScanTest, Fraction2) {
    Buf input = STR_LITERAL(" 1.23");
    JsonToken tokens[] = {
        {.type = JsonToken_Number, .value = STR_LITERAL("1.23")},
    };
    run_json_scan_test(input, tokens, ARRAY_SIZE(tokens), {});
}

TEST(JsonScanTest, Exponent) {
    Buf input = STR_LITERAL(" 1e23 ");
    JsonToken tokens[] = {
        {.type = JsonToken_Number, .value = STR_LITERAL("1e23")},
    };
    run_json_scan_test(input, tokens, ARRAY_SIZE(tokens), {});
}

TEST(JsonScanTest, Exponent2) {
    Buf input = STR_LITERAL(" 1E23");
    JsonToken tokens[] = {
        {.type = JsonToken_Number, .value = STR_LITERAL("1E23")},
    };
    run_json_scan_test(input, tokens, ARRAY_SIZE(tokens), {});
}

TEST(JsonScanTest, ObjectBeginEnd) {
    Buf input = STR_LITERAL("{}");
    JsonToken tokens[] = {
        {.type = JsonToken_ObjectStart},
        {.type = JsonToken_ObjectEnd},
    };
    run_json_scan_test(input, tokens, ARRAY_SIZE(tokens), {});
}

TEST(JsonScanTest, ArrayBeginEnd) {
    Buf input = STR_LITERAL(" [] ");
    JsonToken tokens[] = {
        {.type = JsonToken_ArrayStart},
        {.type = JsonToken_ArrayEnd},
    };
    run_json_scan_test(input, tokens, ARRAY_SIZE(tokens), {});
}

TEST(JsonScanTest, Colon) {
    Buf input = STR_LITERAL(" : ");
    JsonToken tokens[] = {
        {.type = JsonToken_Colon},
    };
    run_json_scan_test(input, tokens, ARRAY_SIZE(tokens), {});
}

TEST(JsonScanTest, Comma) {
    Buf input = STR_LITERAL(" , ");
    JsonToken tokens[] = {
        {.type = JsonToken_Comma},
    };
    run_json_scan_test(input, tokens, ARRAY_SIZE(tokens), {});
}

TEST(JsonScanTest, True) {
    Buf input = STR_LITERAL(" true ");
    JsonToken tokens[] = {
        {.type = JsonToken_True},
    };
    run_json_scan_test(input, tokens, ARRAY_SIZE(tokens), {});
}

TEST(JsonScanTest, False) {
    Buf input = STR_LITERAL(" false ");
    JsonToken tokens[] = {
        {.type = JsonToken_False},
    };
    run_json_scan_test(input, tokens, ARRAY_SIZE(tokens), {});
}

TEST(JsonScanTest, Null) {
    Buf input = STR_LITERAL(" null ");
    JsonToken tokens[] = {
        {.type = JsonToken_Null},
    };
    run_json_scan_test(input, tokens, ARRAY_SIZE(tokens), {});
}
