#include "src/json.h"

#include <gtest/gtest.h>

static void RunJsonTokenizerTest(const char *inputs[], int input_count,
                                 JsonToken tokens[], int token_count) {
  JsonTokenizer tok = InitJsonTokenizer();
  int token_index = 0;
  for (int i = 0; i < input_count; ++i) {
    SetJsonTokenizerInput(&tok,
                          {.data = (u8 *)inputs[i], .size = strlen(inputs[i])});

    JsonToken token = GetNextJsonToken(&tok);
    switch (token.type) {
      case kJsonTokenEof: {
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

  ASSERT_EQ(token_index, token_count)
      << "The number of generated Tokens is less than expected";

  DeinitJsonTokenizer(&tok);
}

TEST(JsonTokenizerTest, String) {
  const char *inputs[] = {" \"a", "b\\", "\\\" "};
  JsonToken tokens[] = {
      {.type = kJsonTokenString, .value = STR_LITERAL("ab\\")},
  };
  RunJsonTokenizerTest(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens));
}

TEST(JsonTokenizerTest, ObjectBeginEnd) {
  const char *inputs[] = {"{", "}"};
  JsonToken tokens[] = {
      {.type = kJsonTokenObjectStart},
      {.type = kJsonTokenObjectEnd},
  };
  RunJsonTokenizerTest(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens));
}

TEST(JsonTokenizerTest, ArrayBeginEnd) {
  const char *inputs[] = {" [", "] "};
  JsonToken tokens[] = {
      {.type = kJsonTokenArrayStart},
      {.type = kJsonTokenArrayEnd},
  };
  RunJsonTokenizerTest(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens));
}

TEST(JsonTokenizerTest, Colon) {
  const char *inputs[] = {" ", ": "};
  JsonToken tokens[] = {
      {.type = kJsonTokenColon},
  };
  RunJsonTokenizerTest(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens));
}

TEST(JsonTokenizerTest, Comma) {
  const char *inputs[] = {" ", ", "};
  JsonToken tokens[] = {
      {.type = kJsonTokenComma},
  };
  RunJsonTokenizerTest(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens));
}

TEST(JsonTokenizerTest, True) {
  const char *inputs[] = {"  t", "ru", "e "};
  JsonToken tokens[] = {
      {.type = kJsonTokenTrue},
  };
  RunJsonTokenizerTest(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens));
}

TEST(JsonTokenizerTest, False) {
  const char *inputs[] = {"  f", "al", "s", "e "};
  JsonToken tokens[] = {
      {.type = kJsonTokenFalse},
  };
  RunJsonTokenizerTest(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens));
}

TEST(JsonTokenizerTest, Null) {
  const char *inputs[] = {"  n", "ul", "l "};
  JsonToken tokens[] = {
      {.type = kJsonTokenNull},
  };
  RunJsonTokenizerTest(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens));
}