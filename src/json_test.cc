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
        ASSERT_EQ(token.type, expected_token->type);
        ASSERT_STREQ((char *)token.value.data,
                     (char *)expected_token->value.data);
        ASSERT_EQ(token.value.size, expected_token->value.size);
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

TEST(JsonTokenizerTest, Object) {
  const char *inputs[] = {"{", "}"};
  JsonToken tokens[] = {
      {.type = kJsonTokenObjectStart},
      {.type = kJsonTokenObjectEnd},
  };
  RunJsonTokenizerTest(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens));
}

TEST(JsonTokenizerTest, Array) {
  const char *inputs[] = {" [", "] "};
  JsonToken tokens[] = {
      {.type = kJsonTokenArrayStart},
      {.type = kJsonTokenArrayEnd},
  };
  RunJsonTokenizerTest(inputs, ARRAY_SIZE(inputs), tokens, ARRAY_SIZE(tokens));
}