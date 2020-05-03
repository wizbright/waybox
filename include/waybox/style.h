#ifndef BOX_STYLE
#define BOX_STYLE

enum {
    sf_solid = 1<<1,
    sf_flat = 1<<2,
    sf_raised = 1<<3,
    sf_diagonal = 1<<4,
    sf_crossdiagonal = 1<<5,
    sf_border = 1<<6,
    sf_bevel = 1<<7,
    sf_bevel1 = 1<<8,
    sf_bevel2 = 1<<9,
    sf_gradient = 1<<10,
    sf_interlaced = 1<<11,
    sf_sunken = 1<<12,
    sf_vertical = 1<<13,
    sf_horizontal = 1<<14
};

struct wb_style {
    // Toolbar
    int toolbar;
    int toolbar_height;
    int toolbar_color;
    int toolbar_colorTo;
    // Buttons
    int toolbar_button;
    int toolbar_button_color;
    int toolbar_button_colorTo;
    int toolbar_button_picColor;
    int toolbar_button_pressed;
    int toolbar_button_pressed_color;
    int toolbar_button_pressed_colorTo;
    int toolbar_button_pressed_picColor;
    
    int toolbar_label;
    int toolbar_label_color;
    int toolbar_label_colorTo;
    int toolbar_label_textColor;
    
    // Workspace
    int toolbar_workspace;
    int toolbar_workspace_pixmap;
    int toolbar_workspace_color;
    int toolbar_workspace_colorTo;
    int toolbar_workspace_textColor;
    int toolbar_workspace_font;

    // Window
    int toolbar_windowLabel;
    int toolbar_windowLabel_color;
    int toolbar_windowLabel_colorTo;
    int toolbar_windowLabel_textColor;
    
    // Clock
    int toolbar_clock;
    int toolbar_clock_pixmap;
    int toolbar_clock_color;
    int toolbar_clock_colorTo;
    int toolbar_clock_textColor;
    int toolbar_clock_font;

    // Iconbar
    int toolbar_iconbar_empty;
    int toolbar_iconbar_empty_pixmap;
    int toolbar_iconbar_empty_color;
    int toolbar_iconbar_empty_colorTo;
    
    int toolbar_iconbar_focused;
    int toolbar_iconbar_focused_pixmap;
    int toolbar_iconbar_focused_color;
    int toolbar_iconbar_focused_colorTo;
    int toolbar_iconbar_focused_textColor;
    int toolbar_iconbar_focused_font;

    // Text
    int toolbar_justify;
    int toolbar_font;

    // Title
    int menu_title;
    int menu_title_color;
    int menu_title_colorTo;
    int menu_title_textColor;
    int menu_title_font;
    int menu_title_justify;
    
    // Frame
    int menu_frame;
    int menu_frame_color;
    int menu_frame_colorTo;
    int menu_frame_textColor;
    int menu_frame_disableColor;
    int menu_frame_font;
    int menu_frame_justify;
    
    // Submenu
    int menu_bullet;
    int menu_bullet_position;
    int menu_submenu_pixmap;
    
    // Highlighted
    int menu_hilite;
    int menu_hilite_color;
    int menu_hilite_colorTo;
    int menu_hilite_textColor;
    int menu_selected_pixmap;
    int menu_unselected_pixmap;

    // Title
    int window_title_focus;
    int window_title_focus_color;
    int window_title_focus_colorTo;
    int window_title_unfocus;
    int window_title_unfocus_color;
    int window_title_unfocus_colorTo;
    int window_title_height;
    
    // Label
    int window_label_focus;
    int window_label_focus_color;
    int window_label_focus_colorTo;
    int window_label_focus_textColor;
    int window_label_unfocus;
    int window_label_unfocus_color;
    int window_label_unfocus_colorTo;
    int window_label_unfocus_textColor;

    int window_handle_focus;
    int window_handle_focus_color;
    int window_handle_focus_colorTo;
    int window_handle_unfocus;
    int window_handle_unfocus_color;
    int window_handle_unfocus_colorTo;

    int window_grip_focus;
    int window_grip_focus_color;
    int window_grip_focus_colorTo;
    int window_grip_unfocus;
    int window_grip_unfocus_color;
    int window_grip_unfocus_colorTo;

    int window_button_focus;
    int window_button_focus_color;
    int window_button_focus_colorTo;
    int window_button_focus_picColor;
    int window_button_unfocus;
    int window_button_unfocus_color;
    int window_button_unfocus_colorTo;
    int window_button_unfocus_picColor;
    int window_button_pressed;
    int window_button_pressed_color;
    int window_button_pressed_colorTo;

    int window_frame_focusColor;
    int window_frame_unfocusColor;

    int window_tab_justify;
    int window_tab_label_unfocus;
    int window_tab_label_unfocus_color;
    int window_tab_label_unfocus_textColor;
    int window_tab_label_focus;
    int window_tab_label_focus_color;
    int window_tab_label_focus_textColor;
    int window_tab_borderWidth;
    int window_tab_borderColor;
    int window_tab_font;;

    int window_font;
    int window_justify;

    // Border
    int borderWidth;
    int borderColor;
    int bevelWidth;
    int handleWidth;
    int frameWidth;
    int Command;
    int rootCommand;
    
    // Old
    int menuFont;
    int titleFont;
    

    int hash;
};

void load_style(struct wb_style *config_style, const char *path);

#endif // BOX_STYLE