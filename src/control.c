#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <linux/videodev2.h>
#include <chiasm.h>

/**
 * @brief List the available user controls on a device.
 *
 * @param device Device to list the controls for.
 * @return 0 on success, -1 on failure.
 */
static int
list_ctrls(struct ch_device *device)
{
    struct ch_ctrls *ctrls = ch_enum_ctrls(device);
    if (ctrls == NULL)
	return (-1);

    size_t idx;
    for (idx = 0; idx < ctrls->length; idx++)
	printf("%s\n", ctrls->ctrls[idx].name);

    ch_destroy_ctrls(ctrls);
    return (0);
}

/**
 * @brief List detailed information about a control on a device.
 *
 * @param device The device to list the control information from.
 * @param control Name of control to detail.
 * @return 0 on success, -1 on failure.
 */
static int
ctrl_info(struct ch_device *device, const char *ctrl_name)
{
    struct ch_ctrl *ctrl = ch_find_ctrl(device, ctrl_name);
    if (ctrl == NULL)
	return (-1);

    int32_t value;
    if (ch_get_ctrl(device, ctrl, &value) == -1) {
	free(ctrl);
	return (-1);
    }

    printf("Information for control \"%s\"\n", ctrl_name);

    switch (ctrl->type) {
    case V4L2_CTRL_TYPE_INTEGER:
	printf("   Type: Integer\n");
	printf("Default: %d\n", ctrl->defval / ctrl->step);
	printf("Current: %d\n", value / ctrl->step);
	printf("  Range: %d / %d\n",
	       ctrl->min / ctrl->step,
	       ctrl->max / ctrl->step);

	break;

    case V4L2_CTRL_TYPE_BOOLEAN:
	printf("   Type: Boolean\n");
	printf("Default: %d\n", ctrl->defval);
	printf("Current: %d\n", value);
	break;

    case V4L2_CTRL_TYPE_MENU: {
	struct ch_ctrl_menu *menu = ch_enum_ctrl_menu(device, ctrl);
	if (menu == NULL) {
	    free(ctrl);
	    return (-1);
	}

	printf("   Type: Menu\n");
	printf("Default: %s\n", menu->items[ctrl->defval].name);
	printf("Current: %s\n", menu->items[value].name);
	printf("Options: ");

	size_t jdx;
	for (jdx = 0; jdx < menu->length; jdx++)
	    if (menu->items[jdx].name[0] != '\0')
		printf("%s%s", menu->items[jdx].name,
		       (jdx != menu->length - 1) ? ", " : "\n");

	ch_destroy_ctrl_menu(menu);
	break;
    }

    default:
	printf("   Type: Unsupported\n");
	break;
    }

    free(ctrl);
    return (0);
}

/**
 * @brief Set the value of a control.
 *
 * @param device Device to set the value for.
 * @param ctrl_name Name of the control to set.
 * @param value Value to set on the control.
 * @return 0 on succes, -1 on failure.
 */
static int
ctrl_set(struct ch_device *device, const char *ctrl_name, const char *value)
{
    struct ch_ctrl *ctrl = ch_find_ctrl(device, ctrl_name);
    if (ctrl == NULL)
	return (-1);

    int32_t ival = 0;

    int r = 0;
    switch (ctrl->type) {
    case V4L2_CTRL_TYPE_INTEGER:
    case V4L2_CTRL_TYPE_BOOLEAN: {
	char *p;
	ival = strtol(value, &p, 10);
	if (p == value) {
	    fprintf(stderr, "Invalid value for control.\n");
	    r = -1;
	}

	break;
    }
    case V4L2_CTRL_TYPE_MENU: {
	struct ch_ctrl_menu *menu = ch_enum_ctrl_menu(device, ctrl);
	if (menu == NULL)
	    break;

	size_t l = strnlen(value, 32);

	size_t idx;
	for (idx = 0; idx < menu->length; idx++)
	    if (strncmp(value, menu->items[idx].name, 32) == 0
		&& strnlen(menu->items[idx].name, 32) == l) {
		ival = (int32_t) idx;
		break;
	    }

	if (idx == menu->length) {
	    fprintf(stderr, "Invalid value for control.\n");
	    r = -1;
	}

	ch_destroy_ctrl_menu(menu);
	break;
    }

    default:
	fprintf(stderr, "Control type not supported.\n");
	r = -1;
	break;
    }

    if (r == 0)
	if ((r = ch_set_ctrl(device, ctrl, ival)) == -1)
	    fprintf(stderr, "Invalid value for control.\n");

    free(ctrl);
    return (r);
}

int
main(int argc, char *argv[])
{
    struct ch_device device;
    bool list = false;
    bool info = false;
    char *ctrl = NULL;
    char *value = NULL;

    ch_init_device(&device);

    int opt;
    while ((opt = getopt(argc, argv, "d:" "s:gi:lh?")) != -1) {
        switch (opt) {
        case 'd':
            if (ch_parse_device_opt(opt, optarg, &device) == -1)
                return (-1);

            break;

        case 'l':
            list = true;
            break;

	case 'i':
	    if (ctrl == NULL)
		ctrl = optarg;
	    else {
		fprintf(stderr, "Argument for -i already given.\n");
		return (-1);
	    }

	    break;

	case 'g':
	    info = true;
	    break;

	case 's':
	    value = optarg;
	    break;

	case 'h':
        case '?':
        default:
            printf(
                "Usage: %s [OPTIONS]\n"
                "Options:\n"
                CH_HELP_D
		" -l   List supported controls and exit.\n"
		" -i   Specify a control for either option -s or -g.\n"
		" -s   Set a value for a control.\n"
		" -g   Get information about a control.\n"
                " -?,h Show this help.\n",
                argv[0]
            );

            return (0);
        }
    }

    // Enable error output to stderr.
    ch_set_stderr(true);

    int r = 0;
    if ((r = ch_open_device(&device)) == -1)
        goto cleanup;

    if (list)
        if ((r = list_ctrls(&device)) == -1)
            goto cleanup;

    if (ctrl) {
	if (info)
	    if ((r = ctrl_info(&device, ctrl)) == -1)
		goto cleanup;
	if (value)
	    if ((r = ctrl_set(&device, ctrl, value)) == -1)
		goto cleanup;
    }

cleanup:
    ch_close_device(&device);
    return (r);
}
