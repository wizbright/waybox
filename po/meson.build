add_project_arguments('-DGETTEXT_PACKAGE="' + meson.project_name().to_lower() + '"',
  '-DLOCALEDIR="' + get_option('prefix') / get_option('localedir') + '"',
  '-DUSE_NLS=1', language: 'c')

i18n = import('i18n', required: false)
if i18n.found()
  i18n.gettext(meson.project_name().to_lower(),
    args: ['--directory=' + source_root,
      '--add-comments=TRANSLATORS',
      '--no-location',
      '--keyword=_',
      '--msgid-bugs=https://github.com/wizbright/waybox/issues']
)
endif
