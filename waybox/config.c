#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "config.h"

static char *parse_xpath_expr(char *expr, xmlXPathContextPtr ctxt) {
	xmlXPathObjectPtr object = xmlXPathEvalExpression((xmlChar *) expr, ctxt);
	if (object == NULL) {
		wlr_log(WLR_INFO, "%s", _("Unable to evaluate expression"));
		xmlXPathFreeContext(ctxt);
		return(NULL);
	}
	if (!object->nodesetval) {
		wlr_log(WLR_INFO, "%s", _("No nodesetval"));
		return NULL;
	}
	xmlChar *ret = NULL;
	if (object->nodesetval->nodeNr > 0)
		ret = xmlNodeGetContent(object->nodesetval->nodeTab[0]);
	xmlXPathFreeObject(object);
	return (char *) ret;
}

static bool parse_key_bindings(struct wb_config *config, xmlXPathContextPtr ctxt)
{
	/* Get the key bindings */
	wl_list_init(&config->key_bindings);
	xmlXPathObjectPtr object = xmlXPathEvalExpression((xmlChar *) "//ob:keyboard/ob:keybind", ctxt);
	if (object == NULL) {
		wlr_log(WLR_INFO, "%s", _("Unable to evaluate expression"));
		return(false);
	}
	if (!object->nodesetval) {
		wlr_log(WLR_INFO, "%s", _("No nodeset"));
		return false;
	}

	/* Iterate through the nodes to get the information */
	if (object->nodesetval->nodeNr > 0) {
		int i;
		for (i = 0; i < object->nodesetval->nodeNr; i++) {
			if (object->nodesetval->nodeTab[i]) {
				/* First get the key combinations */
				xmlAttr *keycomb = object->nodesetval->nodeTab[i]->properties;
				while (strcmp((char *) keycomb->name, "key") != 0)
				{
					keycomb = keycomb->next;
				}

				char *sym;
				uint32_t modifiers = 0;
				sym = (char *) keycomb->children->content;
				char *s;

				struct wb_key_binding *key_bind = calloc(1, sizeof(struct wb_key_binding));
				key_bind->sym = 0;
				key_bind->modifiers = 0;
				while ((s = strtok(sym, "-")) != NULL)
				{
					if (strcmp(s, "A") == 0 || strcmp(s, "Alt") == 0)
						modifiers |= WLR_MODIFIER_ALT;
					else if (strcmp(s, "Caps") == 0)
						modifiers |= WLR_MODIFIER_CAPS;
					else if (strcmp(s, "C") == 0 || strcmp(s, "Ctrl") == 0)
						modifiers |= WLR_MODIFIER_CTRL;
					else if (strcmp(s, "Mod2") == 0)
						modifiers |= WLR_MODIFIER_MOD2;
					else if (strcmp(s, "Mod3") == 0)
						modifiers |= WLR_MODIFIER_MOD3;
					else if (strcmp(s, "Mod5") == 0)
						modifiers |= WLR_MODIFIER_MOD5;
					else if (strcmp(s, "S") == 0 || strcmp(s, "Shift") == 0)
						modifiers |= WLR_MODIFIER_SHIFT;
					else if (strcmp(s, "W") == 0 || strcmp(s, "Logo") == 0)
						modifiers |= WLR_MODIFIER_LOGO;
					else
						key_bind->sym = xkb_keysym_from_name(s, 0);
					key_bind->modifiers = modifiers;
					sym = NULL;
				}

				/* Now get the actions */
				xmlNode *cur_node;
				xmlNode *new_node = object->nodesetval->nodeTab[i]->children;
				xmlAttr *attr;
				for (cur_node = new_node; cur_node; cur_node = cur_node->next)
				{
					if (strcmp((char *) cur_node->name, "action") == 0)
					{
						attr = cur_node->properties;
						while (strcmp((char *) attr->name, "name") != 0)
						{
							attr = attr->next;
						}
						key_bind->action = malloc(sizeof (char) * 64);
						strcpy(key_bind->action, (char *) attr->children->content);
						wlr_log(WLR_INFO, "Registering action %s", (char *) key_bind->action);
						if (strcmp((char *) key_bind->action, "Execute") != 0)
							break;
						cur_node = cur_node->children;
					}
					if (strcmp((char *) cur_node->name, "execute") == 0)
					{
						/* Bad things can happen if the command is greater than 1024 characters */
						key_bind->cmd = malloc(sizeof(char) * 1024);
						strncpy(key_bind->cmd, (char *) cur_node->children->content, 1023);
						key_bind->cmd[strlen(key_bind->cmd)] = '\0';
						if (key_bind->action)
							break;
					}
				}

				wl_list_insert(&config->key_bindings, &key_bind->link);
			}
		}
	}
	xmlXPathFreeObject(object);
	return true;
}

bool init_config(struct wb_server *server)
{
	xmlDocPtr doc;
	if (server->config_file == NULL) {
		char *xdg_config = getenv("XDG_CONFIG_HOME");
		if (!xdg_config)
			xdg_config = "~/.config";

		char *rc_file = malloc(strlen(xdg_config) + 14);
		strcpy(rc_file, xdg_config);
		rc_file = strcat(rc_file, "/waybox/rc.xml");
		doc = xmlParseFile(rc_file);
		wlr_log(WLR_INFO, "Using config file %s", rc_file);
		free(rc_file);
	} else {
		wlr_log(WLR_INFO, "Using config file %s", server->config_file);
		doc = xmlParseFile(server->config_file);
	}

	if (doc == NULL) {
		wlr_log(WLR_INFO, "%s", _("Unable to parse XML file"));
		return false;
	}
	xmlXPathContextPtr ctxt = xmlXPathNewContext(doc);
	if (ctxt == NULL) {
		wlr_log(WLR_INFO, "%s", _("Couldn't create new context!"));
		xmlFreeDoc(doc);
		return(false);
	}
	if (xmlXPathRegisterNs(ctxt, (const xmlChar *) "ob", (const xmlChar *) "http://openbox.org/3.4/rc") != 0) {
		wlr_log(WLR_INFO, "%s", _("Couldn't register the namespace"));
	}

	struct wb_config *config = calloc(1, sizeof(struct wb_config));
	config->keyboard_layout.layout = parse_xpath_expr("//ob:keyboard//ob:layout//ob:layout", ctxt);
	config->keyboard_layout.model = parse_xpath_expr("//ob:keyboard//ob:layout//ob:model", ctxt);
	config->keyboard_layout.options = parse_xpath_expr("//ob:keyboard//ob:layout//ob:options", ctxt);
	config->keyboard_layout.rules = parse_xpath_expr("//ob:keyboard//ob:layout//ob:rules", ctxt);
	config->keyboard_layout.variant = parse_xpath_expr("//ob:keyboard//ob:layout//ob:variant", ctxt);
	if (!parse_key_bindings(config, ctxt))
	{
		xmlFreeDoc(doc);
		return false;
	}
	server->config = config;

	xmlXPathFreeContext(ctxt);
	xmlFreeDoc(doc);
	xmlCleanupParser();

	return true;
}

void deinit_config(struct wb_config *config)
{
	/* Free everything allocated in init_config */
	struct wb_key_binding *key_binding;
	wl_list_for_each(key_binding, &config->key_bindings, link) {
		free(key_binding->action);
		free(key_binding->cmd);
		free(key_binding);
	}
	wl_list_remove(&config->key_bindings);
	free(config);
}
