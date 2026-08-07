class SlideShow
  ROW_HEIGHT = 16 unless const_defined?(:ROW_HEIGHT)
  BODY_TOP = 18 unless const_defined?(:BODY_TOP)
  BODY_BOTTOM = 112 unless const_defined?(:BODY_BOTTOM)
  VISIBLE_ROW_COUNT = (BODY_BOTTOM - BODY_TOP) / ROW_HEIGHT unless const_defined?(:VISIBLE_ROW_COUNT)

  TEXT_COLOR = 0xCFA45F unless const_defined?(:TEXT_COLOR)
  HEADING_1_COLOR = 0xFFD966 unless const_defined?(:HEADING_1_COLOR)
  HEADING_2_COLOR = 0xF5972D unless const_defined?(:HEADING_2_COLOR)
  HEADING_3_COLOR = 0xCFA45F unless const_defined?(:HEADING_3_COLOR)
  CODE_COLOR = 0xFFD966 unless const_defined?(:CODE_COLOR)
  CODE_BACKGROUND_COLOR = 0x241604 unless const_defined?(:CODE_BACKGROUND_COLOR)
end
