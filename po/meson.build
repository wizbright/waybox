i18n = import('i18n')
add_project_arguments('-DGETTEXT_PACKAGE="' + meson.project_name().to_lower() + '"',
  '-DLOCALEDIR="' + get_option('prefix') + '/' +  get_option('localedir') + '"',
  language:'c')
i18n.gettext(meson.project_name().to_lower(),
  args: ['--directory=' + meson.source_root(),
    '--add-comments=TRANSLATORS',
    '--msgid-bugs=https://github.com/wizbright/waybox/issues']
)
