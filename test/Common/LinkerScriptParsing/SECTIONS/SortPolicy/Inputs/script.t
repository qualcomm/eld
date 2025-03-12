SECTIONS {
  FOO : { *(SORT_BY_NAME(*foo*)) }
  FOO_FOO : { *(SORT_BY_NAME(SORT_BY_NAME(*foo_foo*))) }
  BAR : { *(SORT_BY_ALIGNMENT(*bar*)) }
  BAR_BAR : { *(SORT_BY_ALIGNMENT(SORT_BY_ALIGNMENT(*bar_bar*))) }
  BAZ : { *(SORT_BY_INIT_PRIORITY(*baz*)) }
  FOO_BAR : { *(SORT_BY_NAME(SORT_BY_ALIGNMENT(*foo_bar*))) }
  BAR_FOO : { *(SORT_BY_ALIGNMENT(SORT_BY_NAME(*bar_foo*))) }
  FRED : { *(SORT_NONE(*none*)) }
}
