#include "src/json.h"

#include <gtest/gtest.h>

static void run_tok_test(const char *inputs[], int input_count,
                         JsonToken tokens[], int token_count) {
    JsonTokenizer tok = json_init_tok();
    int token_index = 0;
    for (int i = 0; i < input_count && json_is_scanning(&tok); ++i) {
        json_set_input(&tok,
                       {.data = (u8 *)inputs[i], .size = strlen(inputs[i])},
                       i + 1 == input_count);

        bool eof = false;
        while (!eof && json_is_scanning(&tok)) {
            JsonToken token = json_get_next_token(&tok);
            switch (token.type) {
                case JsonToken_Eof: {
                    eof = true;
                } break;

                default: {
                    ASSERT_LT(token_index, token_count);
                    JsonToken *expected_token = &tokens[token_index++];
                    ASSERT_STREQ((char *)token.value.data,
                                 (char *)expected_token->value.data);
                    ASSERT_EQ(token.value.size, expected_token->value.size);
                    ASSERT_EQ(token.type, expected_token->type);
                } break;
            }
        }
    }

    ASSERT_EQ(token_index, token_count)
        << "The number of generated Tokens is less than expected";

    json_deinit_tok(&tok);
}

TEST(JsonTokenizerTest, StringEscape) {
    const char *inputs[] = {
        " \"a",
        "b\\",
        "\\\" ",
    };
    JsonToken tokens[] = {
        {.type = JsonToken_String, .value = STR_LITERAL("ab\\\\")},
    };
    run_tok_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens));
}

TEST(JsonTokenizerTest, StringEscapeU) {
    const char *inputs[] = {
        "\"\\uabcd\"",
    };
    JsonToken tokens[] = {
        {.type = JsonToken_String, .value = STR_LITERAL("\\uabcd")},
    };
    run_tok_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens));
}

TEST(JsonTokenizerTest, StringEof) {
    const char *inputs[] = {
        " \"a",
        " ",
    };
    JsonToken tokens[] = {
        {.type = JsonToken_Error,
         .value = STR_LITERAL(
             "End of string '\"' expected but reached end of input")},
    };
    run_tok_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens));
}

TEST(JsonTokenizerTest, Integer) {
    const char *inputs[] = {
        " 1",
        "2",
        "3 ",
    };
    JsonToken tokens[] = {
        {.type = JsonToken_Number, .value = STR_LITERAL("123")},
    };
    run_tok_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens));
}

TEST(JsonTokenizerTest, Fraction) {
    const char *inputs[] = {
        " 1.",
        "2",
        "3 ",
    };
    JsonToken tokens[] = {
        {.type = JsonToken_Number, .value = STR_LITERAL("1.23")},
    };
    run_tok_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens));
}

TEST(JsonTokenizerTest, Exponent) {
    const char *inputs[] = {" 1e", "2", "3 4", "E56"};
    JsonToken tokens[] = {
        {.type = JsonToken_Number, .value = STR_LITERAL("1e23")},
        {.type = JsonToken_Number, .value = STR_LITERAL("4E56")},
    };
    run_tok_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens));
}

TEST(JsonTokenizerTest, ObjectBeginEnd) {
    const char *inputs[] = {
        "{",
        "}",
    };
    JsonToken tokens[] = {
        {.type = JsonToken_ObjectStart},
        {.type = JsonToken_ObjectEnd},
    };
    run_tok_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens));
}

TEST(JsonTokenizerTest, ArrayBeginEnd) {
    const char *inputs[] = {
        " [",
        "] ",
    };
    JsonToken tokens[] = {
        {.type = JsonToken_ArrayStart},
        {.type = JsonToken_ArrayEnd},
    };
    run_tok_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens));
}

TEST(JsonTokenizerTest, Colon) {
    const char *inputs[] = {
        " ",
        ": ",
    };
    JsonToken tokens[] = {
        {.type = JsonToken_Colon},
    };
    run_tok_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens));
}

TEST(JsonTokenizerTest, Comma) {
    const char *inputs[] = {" ", ", "};
    JsonToken tokens[] = {
        {.type = JsonToken_Comma},
    };
    run_tok_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens));
}

TEST(JsonTokenizerTest, True) {
    const char *inputs[] = {"  t", "ru", "e "};
    JsonToken tokens[] = {
        {.type = JsonToken_True},
    };
    run_tok_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens));
}

TEST(JsonTokenizerTest, False) {
    const char *inputs[] = {"  f", "al", "s", "e "};
    JsonToken tokens[] = {
        {.type = JsonToken_False},
    };
    run_tok_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens));
}

TEST(JsonTokenizerTest, Null) {
    const char *inputs[] = {"  n", "ul", "l "};
    JsonToken tokens[] = {
        {.type = JsonToken_Null},
    };
    run_tok_test(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens));
}