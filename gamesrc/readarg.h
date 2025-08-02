#pragma once

#include <stddef.h>

#define READARG_STRINGS(...) ((char *[]){__VA_ARGS__, NULL})

enum readarg_error {
    READARG_ESUCCESS,
    READARG_ENOVAL,
    READARG_ENOTREQ,
    READARG_ENOTOPT,
    READARG_ERANGEOPT,
    READARG_ERANGEOPER,
};

enum readarg_form {
    READARG_FORM_SHORT,
    READARG_FORM_LONG,
};

struct readarg_view_strings {
    const char **strings;
    size_t len;
};

struct readarg_bounds {
    /* The upper value will be ignored if inf is non-zero. */
    size_t val[2];
    int inf;
};

/* An argument in this case may either be an option argument or an operand. */
struct readarg_arg {
    char *name;
    struct readarg_bounds bounds;
    struct readarg_view_strings val;
};

struct readarg_opt {
    /* Two null-terminated arrays of either long or short option names. */
    char **names[2];
    struct readarg_arg arg;
};

struct readarg_parser {
    size_t nopts;
    struct readarg_opt *opts;
    size_t nopers;
    struct readarg_arg *opers;
    struct readarg_view_strings args;
    struct {
        int pending;
        const char *grppos;
        struct {
            struct readarg_opt *opt;
            /* Reference to the current argument being parsed. */
            const char **arg;
            /* Reference to the last element of the option/operand value view. */
            const char **eoval;
            /* Intermediate operands which have not yet been assigned. */
            struct readarg_view_strings ioper;
        } curr;
    } state;
    enum readarg_error error;
};

struct readarg_helpgen_writer {
    /* A falsy return value should indicate to the caller that an error occured. */
    int (*write)(void *ctx, const char *buf, size_t len);
    void *ctx;
};

/* Iteratively parse the arguments. */
int readarg_parse(struct readarg_parser *rp);
/* args should always exclude the first element. */
void readarg_parser_init(struct readarg_parser *rp, struct readarg_opt *opts, size_t nopts, struct readarg_arg *opers, size_t nopers, struct readarg_view_strings args);
/* Output usage information. */
int readarg_helpgen_put_usage(struct readarg_parser *rp, struct readarg_helpgen_writer *writer, const char *progname, const char *usage);
/* Assign operands from the operand list to operands defined for the parser. */
void readarg_assign_opers(struct readarg_parser *rp);
/* Validate that all options meet their requirements. */
struct readarg_opt *readarg_validate_opts(struct readarg_parser *rp);
/* Check whether the argument's values are within the defined limits. */
int readarg_validate_arg(struct readarg_arg *arg);
/* Get the upper limit. */
size_t readarg_select_upper(struct readarg_bounds bounds);
/* Get the lower limit. This does not always return the minimum. */
size_t readarg_select_lower(struct readarg_bounds bounds);

#ifdef READARG_IMPLEMENTATION

#ifdef READARG_DEBUG
#pragma push_macro("NDEBUG")
#undef NDEBUG
#endif

#define READARG_HELPGEN_TRY_BUF(writer, buf, len)                              \
    do {                                                                       \
        int readarg_helpgen_rv = (writer)->write((writer)->ctx, (buf), (len)); \
        if (!readarg_helpgen_rv)                                               \
            return readarg_helpgen_rv;                                         \
    } while (0)
#define READARG_HELPGEN_TRY_STR(writer, s) READARG_HELPGEN_TRY_BUF((writer), (s), (strlen((s))))

#include <assert.h>
#include <string.h>

static void readarg_parse_arg(struct readarg_parser *rp, const char *arg);

static void readarg_parse_opt(struct readarg_parser *rp, enum readarg_form form, const char **pos);

static struct readarg_opt *readarg_match_opt(struct readarg_parser *rp, enum readarg_form form, const char **needle);

static void readarg_update_opt(struct readarg_parser *rp, const char *attach, struct readarg_opt *opt);
static void readarg_update_oper(struct readarg_parser *rp, struct readarg_view_strings val);

static void readarg_add_val(struct readarg_parser *rp, struct readarg_arg *arg, const char *string, int end);

static const char *readarg_skip_incl(const char *outer, const char *inner);

static void readarg_occ_opt(struct readarg_parser *rp, struct readarg_opt *opt);

static void readarg_permute_val(struct readarg_parser *rp, struct readarg_view_strings *target, const char *val, int end);
static void readarg_incr_between(const char **start, const char **stop, struct readarg_view_strings *curr, struct readarg_view_strings *exclude);
static void readarg_permute_rest(const char **target, struct readarg_view_strings start);

int readarg_parse(struct readarg_parser *rp) {
    /* Check whether the current offset is at the end of argv. */
    size_t off = rp->state.curr.arg - rp->args.strings;
    if (off >= rp->args.len) {
        if (rp->state.pending)
            /* The last specified option required an argument, but no argument has been provided. */
            rp->error = READARG_ENOVAL;

        return 0;
    }

    if (rp->state.pending) {
        readarg_add_val(rp, &rp->state.curr.opt->arg, *rp->state.curr.arg, 0);
        ++rp->state.curr.arg;
        return !rp->error;
    }

    readarg_parse_arg(rp, *rp->state.curr.arg);

    /* If grouped options are still being parsed, they should not be discarded. */
    if (!rp->state.grppos)
        ++rp->state.curr.arg;

    return !rp->error;
}

void readarg_parser_init(struct readarg_parser *rp, struct readarg_opt *opts, size_t nopts, struct readarg_arg *opers, size_t nopers, struct readarg_view_strings args) {
    *rp = (struct readarg_parser){
        .args = args,
        .opts = opts,
        .nopts = nopts,
        .opers = opers,
        .nopers = nopers,
        .state.curr = {
            .arg = args.strings,
            .eoval = args.strings,
        },
    };
}

int readarg_helpgen_put_usage(struct readarg_parser *rp, struct readarg_helpgen_writer *writer, const char *progname, const char *usage) {
    READARG_HELPGEN_TRY_STR(writer, usage);
    READARG_HELPGEN_TRY_STR(writer, ":\n");

    READARG_HELPGEN_TRY_STR(writer, progname);
    READARG_HELPGEN_TRY_STR(writer, "\n");

    int optwritten = 0, operwritten = 0;
    int next;

    struct readarg_opt *opts = rp->opts;
    next = !!rp->nopts;
    for (size_t i = 0; next; i++) {
        optwritten = 1;

        if (i == 0)
            READARG_HELPGEN_TRY_STR(writer, "  ");

        next = i + 1 < rp->nopts;
        size_t lower = readarg_select_lower(opts[i].arg.bounds);
        size_t upper = readarg_select_upper(opts[i].arg.bounds);
        int inf = opts[i].arg.bounds.inf;
        size_t nforms = sizeof opts[i].names / sizeof *opts[i].names;

        for (size_t j = 0; j < (upper ? upper : !!inf); j++) {
            if (j >= lower)
                READARG_HELPGEN_TRY_STR(writer, "[");

            for (size_t k = 0; k < nforms; k++) {
                int grp = 0;
                if (opts[i].names[k]) {
                    for (size_t l = 0; opts[i].names[k][l]; l++) {
                        if (!grp) {
                            if (k == READARG_FORM_SHORT) {
                                READARG_HELPGEN_TRY_STR(writer, "-");
                            }

                            if (k == READARG_FORM_LONG) {
                                READARG_HELPGEN_TRY_STR(writer, "--");
                            }
                        }

                        READARG_HELPGEN_TRY_STR(writer, opts[i].names[k][l]);

                        if (k == READARG_FORM_SHORT) {
                            grp = 1;
                            if (!opts[i].names[k][l + 1])
                                READARG_HELPGEN_TRY_STR(writer, ", ");
                            continue;
                        } else if (k + 1 < nforms || opts[i].names[k][l + 1]) {
                            READARG_HELPGEN_TRY_STR(writer, ", ");
                        } else if (opts[i].arg.name) {
                            READARG_HELPGEN_TRY_STR(writer, " ");
                            READARG_HELPGEN_TRY_STR(writer, opts[i].arg.name);

                            if (inf)
                                READARG_HELPGEN_TRY_STR(writer, "...");
                        }
                    }
                }
            }

            if (j >= lower)
                READARG_HELPGEN_TRY_STR(writer, "]");

            if (next)
                READARG_HELPGEN_TRY_STR(writer, "\n  ");
        }
    }

    if (optwritten)
        READARG_HELPGEN_TRY_STR(writer, "\n");

    struct readarg_arg *opers = rp->opers;
    next = !!rp->nopers;
    for (size_t i = 0; next; i++) {
        operwritten = 1;

        if (i == 0)
            READARG_HELPGEN_TRY_STR(writer, "  ");

        next = i + 1 < rp->nopers;
        size_t lower = readarg_select_lower(opers[i].bounds);
        size_t upper = readarg_select_upper(opers[i].bounds);
        int inf = opers[i].bounds.inf;

        for (size_t j = 0; j < lower; j++) {
            READARG_HELPGEN_TRY_STR(writer, opers[i].name);

            if (inf && j + 1 == lower)
                READARG_HELPGEN_TRY_STR(writer, "...");

            if (next)
                READARG_HELPGEN_TRY_STR(writer, "\n  ");
        }

        size_t amt = upper ? upper : inf ? lower + 1 : 0;
        for (size_t j = lower; j < amt; j++) {
            READARG_HELPGEN_TRY_STR(writer, "[");

            READARG_HELPGEN_TRY_STR(writer, opers[i].name);

            if (inf && j + 1 == amt)
                READARG_HELPGEN_TRY_STR(writer, "...");

            READARG_HELPGEN_TRY_STR(writer, "]");

            if (next)
                READARG_HELPGEN_TRY_STR(writer, "\n  ");
        }
    }

    if (operwritten)
        READARG_HELPGEN_TRY_STR(writer, "\n");

    return 1;
}


void readarg_assign_opers(struct readarg_parser *rp) {
    size_t count = rp->state.curr.ioper.len;

    size_t nlower = 0;
    for (size_t i = 0; i < rp->nopers; i++)
        nlower += readarg_select_lower(rp->opers[i].bounds);

    if (count < nlower) {
        rp->error = READARG_ERANGEOPER;
        return;
    }

    struct {
        size_t extra;
        size_t req;
    } rest = {
        count - nlower,
        nlower,
    };

    for (size_t i = 0; i < rp->nopers; i++) {
        if (count == 0 || !rp->opers[i].val.strings) {
            size_t off = count - (rest.extra + rest.req);
            rp->opers[i].val.strings = rp->state.curr.ioper.strings + off;
        }

        size_t lower = readarg_select_lower(rp->opers[i].bounds);
        size_t upper = readarg_select_upper(rp->opers[i].bounds);
        int inf = rp->opers[i].bounds.inf;

        size_t add;

        /* Add required elements. */
        add = rest.req > lower ? lower : rest.req;
        rp->opers[i].val.len += add, rest.req -= add;

        /* Add optional elements. */
        add = inf ? rest.extra : rest.extra > upper ? upper : rest.extra;
        rp->opers[i].val.len += add, rest.extra -= add;
    }

    if (rest.extra || rest.req)
        rp->error = READARG_ERANGEOPER;
}

struct readarg_opt *readarg_validate_opts(struct readarg_parser *rp) {
    for (size_t i = 0; i < rp->nopts; i++) {
        if (!readarg_validate_arg(&rp->opts[i].arg)) {
            rp->error = READARG_ERANGEOPT;
            return &rp->opts[i];
        }
    }

    return NULL;
}

int readarg_validate_arg(struct readarg_arg *arg) {
    size_t occ = arg->val.len;
    size_t upper = readarg_select_upper(arg->bounds);
    size_t lower = readarg_select_lower(arg->bounds);
    return occ >= lower && (occ <= upper || arg->bounds.inf);
}

size_t readarg_select_upper(struct readarg_bounds bounds) {
    return bounds.val[0] > bounds.val[1] ? bounds.val[0] : bounds.val[1];
}

size_t readarg_select_lower(struct readarg_bounds bounds) {
    return bounds.inf ? readarg_select_upper(bounds) : bounds.val[0] < bounds.val[1] ? bounds.val[0] : bounds.val[1];
}

static void readarg_parse_arg(struct readarg_parser *rp, const char *arg) {
    /* Parse the next option in the grouped option string, which automatically advances it. */
    if (rp->state.grppos) {
        readarg_parse_opt(rp, READARG_FORM_SHORT, &rp->state.grppos);
        if (!*rp->state.grppos)
            rp->state.grppos = NULL;
        return;
    }

    const char *pos = arg;

    switch (*pos) {
    case '-':
        ++pos;
        switch (*pos) {
        case '-':
            ++pos;
            switch (*pos) {
                size_t off;
            case '\0':
                /* "--" denotes the end of options. */
                off = rp->args.len - (rp->state.curr.arg - rp->args.strings);
                assert(off);
                if (off == 1)
                    /* No operands after the "--". */
                    return;

                readarg_update_oper(rp, (struct readarg_view_strings){
                                            .len = off - 1,
                                            .strings = rp->state.curr.arg + 1,
                                        });
                rp->state.curr.arg = rp->args.strings + rp->args.len - 1;

                return;
            default:
                readarg_parse_opt(rp, READARG_FORM_LONG, &pos);
                return;
            }
        case '\0':
            readarg_update_oper(rp, (struct readarg_view_strings){.len = 1, .strings = (const char *[]){arg}});
            return;
        default:
            readarg_parse_opt(rp, READARG_FORM_SHORT, &pos);
            return;
        }
    default:
        readarg_update_oper(rp, (struct readarg_view_strings){.len = 1, .strings = (const char *[]){arg}});
        return;
    }
}

static void readarg_parse_opt(struct readarg_parser *rp, enum readarg_form form, const char **pos) {
    struct readarg_opt *match;
    assert(form == READARG_FORM_SHORT || form == READARG_FORM_LONG);

    if (form == READARG_FORM_SHORT) {
        match = readarg_match_opt(rp, form, pos);
        if (match) {
            const char *strpos = *pos;

            if (!match->arg.name && *strpos) {
                rp->state.grppos = strpos;
                readarg_update_opt(rp, NULL, match);
            } else {
                readarg_update_opt(rp, *strpos ? strpos : NULL, match);
            }
        } else {
            rp->error = READARG_ENOTOPT;
        }
    } else {
        /* Match and advance pos to the end of the match. */
        match = readarg_match_opt(rp, form, pos);

        if (match) {
            switch (**pos) {
            case '\0':
                readarg_update_opt(rp, NULL, match);
                break;
            case '=':
                ++(*pos);
                readarg_update_opt(rp, *pos, match);
                break;
            default:
                rp->error = READARG_ENOTOPT;
                break;
            }
        } else {
            rp->error = READARG_ENOTOPT;
        }
    }
}

static struct readarg_opt *readarg_match_opt(struct readarg_parser *rp, enum readarg_form form, const char **needle) {
    /* This represents the last inexact match. */
    struct {
        /* The current advanced string. */
        const char *adv;
        /* The current option. */
        struct readarg_opt *opt;
    } loose = {0};

    for (size_t i = 0; i < rp->nopts; i++) {
        /* Iterate through all short or long names of the current option. */
        char **names = rp->opts[i].names[form];

        if (!names)
            /* Ignore the option as it does not have names in the required form. */
            continue;

        const char *cmp = loose.adv;

        for (size_t j = 0; names[j]; j++) {
            char *name = names[j];
            cmp = readarg_skip_incl(*needle, name);

            if (!cmp)
                continue;

            if (!*cmp) {
                /* A guaranteed match. */
                *needle = cmp;
                return rp->state.curr.opt = rp->opts + i;
            } else if ((cmp - *needle) > (loose.adv - *needle))
                /* Maybe a match, maybe not. */
                loose.adv = cmp, loose.opt = rp->opts + i;
        }
    }

    if (loose.adv)
        *needle = loose.adv;

    if (loose.opt)
        return rp->state.curr.opt = loose.opt;

    return NULL;
}

static void readarg_update_opt(struct readarg_parser *rp, const char *attach, struct readarg_opt *opt) {
    if (opt->arg.name) {
        if (attach) {
            /* --opt=value, --opt=, -ovalue */
            struct readarg_arg *curr = &rp->state.curr.opt->arg;
            readarg_occ_opt(rp, opt);
            readarg_add_val(rp, curr, attach, 0);
        } else {
            /* --opt value, -o value */
            rp->state.pending = 1;
            readarg_occ_opt(rp, opt);
        }
    } else {
        readarg_occ_opt(rp, opt);
        if (attach)
            rp->error = READARG_ENOTREQ;
    }
}

static void readarg_update_oper(struct readarg_parser *rp, struct readarg_view_strings val) {
    assert(val.len && val.strings);

    if (val.len == 1) {
        ++rp->state.curr.ioper.len;
        readarg_permute_val(rp, &rp->state.curr.ioper, val.strings[0], 1);
    } else {
        readarg_permute_rest(rp->state.curr.eoval, val);
        rp->state.curr.ioper.len += val.len;
    }
}

static void readarg_add_val(struct readarg_parser *rp, struct readarg_arg *arg, const char *string, int end) {
    rp->state.pending = 0;

    if (!readarg_validate_arg(arg))
        rp->error = READARG_ERANGEOPT;
    else
        readarg_permute_val(rp, &arg->val, string, end);
}

static const char *readarg_skip_incl(const char *outer, const char *inner) {
    for (; *inner && *inner == *outer; ++inner, ++outer);
    return !*inner ? outer : NULL;
}

static void readarg_occ_opt(struct readarg_parser *rp, struct readarg_opt *opt) {
    assert(opt);
    rp->state.curr.opt = opt;
    ++rp->state.curr.opt->arg.val.len;
}

static void readarg_permute_val(struct readarg_parser *rp, struct readarg_view_strings *target, const char *val, int end) {
    if (!target->strings)
        /* Fallback position when no value has yet been set. */
        target->strings = rp->state.curr.eoval - (end ? 0 : rp->state.curr.ioper.len);

    const char **pos = target->strings + target->len - 1;

    assert(rp->state.curr.arg >= rp->state.curr.eoval);

    memmove(pos + 1, pos, (rp->state.curr.eoval - pos) * sizeof *pos);

    *pos = val;
    ++rp->state.curr.eoval;

    const char **start = pos, **stop = rp->state.curr.eoval;

    /* Increment all value pointers in the options which are between start and stop (inclusive). */
    for (size_t i = 0; i < rp->nopts; i++)
        readarg_incr_between(start, stop, &rp->opts[i].arg.val, target);

    readarg_incr_between(start, stop, &rp->state.curr.ioper, target);
}

static void readarg_incr_between(const char **start, const char **stop, struct readarg_view_strings *curr, struct readarg_view_strings *exclude) {
    if (curr->strings >= start && curr->strings <= stop && curr != exclude)
        ++curr->strings;
}

static void readarg_permute_rest(const char **target, struct readarg_view_strings start) {
    memmove(target, start.strings, start.len * sizeof *start.strings);
}

#ifdef READARG_DEBUG
#pragma pop_macro("NDEBUG")
#endif

#endif
