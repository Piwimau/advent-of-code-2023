#define SCU_SHORT_ALIASES

#include <scu/scu.h>
#include <stdlib.h>

/** @brief Represents a type of rule in a workflow. */
typedef enum RuleType {
    RULE_TYPE_CMP_LT,
    RULE_TYPE_CMP_GT,
    RULE_TYPE_FWD
} RuleType;

/** @brief Represents a category of part ratings. */
typedef enum Category {
    CATEGORY_X,
    CATEGORY_M,
    CATEGORY_A,
    CATEGORY_S
} Category;

/**
 * @brief The maximum length of workflow identifiers (including the terminating
 * null byte).
 */
static constexpr isize MAX_ID_LENGTH = 8;

/** @brief Represents a rule in a workflow. */
typedef struct Rule {

    /** @brief The type of the rule. */
    RuleType type;

    /**
     * @brief The category of the part rating to compare against.
     *
     * @note This field is only meaningful for comparison rules (i.e.,
     * `RULE_TYPE_CMP_LT` and `RULE_TYPE_CMP_GT`).
     */
    Category category;

    /**
     * @brief The threshold value for the rule.
     *
     * @note This field is only meaningful for comparison rules (i.e.,
     * `RULE_TYPE_CMP_LT` and `RULE_TYPE_CMP_GT`).
     */
    isize threshold;

    /** @brief The identifier of the next workflow to transition to. */
    char next[MAX_ID_LENGTH];

} Rule;

/** @brief Represents a workflow consisting of a sequence of rules. */
typedef struct Workflow {

    /** @brief The identifier of the workflow. */
    char id[MAX_ID_LENGTH];

    /** @brief The sequence of rules in the workflow. */
    Rule* rules;

} Workflow;

/** @brief Represents a part rating consisting of four category ratings. */
typedef struct Rating {

    /** @brief The rating in the `x` category. */
    isize x;

    /** @brief The rating in the `m` category. */
    isize m;

    /** @brief The rating in the `a` category. */
    isize a;

    /** @brief The rating in the `s` category. */
    isize s;

} Rating;

/** @brief Represents a range of values. */
typedef struct Range {

    /** @brief The lower bound of the range (inclusive). */
    isize low;

    /** @brief The upper bound of the range (inclusive). */
    isize high;

} Range;

/** @brief Represents a set of ranges for the categories of part ratings. */
typedef struct RangeSet {

    /** @brief The range of values for the `x` category. */
    Range x;

    /** @brief The range of values for the `m` category. */
    Range m;

    /** @brief The range of values for the `a` category. */
    Range a;

    /** @brief The range of values for the `s` category. */
    Range s;

} RangeSet;

/** @brief The minimum possible value for a part rating. */
static constexpr isize RATING_MIN = 1;

/** @brief The maximum possible value for a part rating. */
static constexpr isize RATING_MAX = 4000;

/**
 * @brief Determines whether two specified rules are equal.
 *
 * @param[in] a A pointer to the first rule.
 * @param[in] b A pointer to the second rule.
 * @return `true` if the specified rules are equal, otherwise `false`.
 */
static bool rule_equal(const Rule* a, const Rule* b) {
    SCU_ASSERT(a);
    SCU_ASSERT(b);
    if (a->type != b->type) {
        return false;
    }
    if (a->type == RULE_TYPE_FWD) {
        return scu_strcmp(a->next, b->next) == 0;
    }
    return (a->category == b->category) && (a->threshold == b->threshold)
        && (scu_strcmp(a->next, b->next) == 0);
}

/**
 * @brief Deallocates a specified workflow.
 *
 * @note If `workflow` is a `nullptr`, this function does nothing.
 *
 * Note that this function only deallocates any resources associated with the
 * workflow, but not the workflow itself. The caller is responsible for
 * deallocating the workflow manually if it was dynamically allocated.
 *
 * @warning The behavior is undefined if `workflow` is used after it has been
 * deallocated.
 *
 * @param[in, out] workflow The workflow to deallocate.
 */
static inline void workflow_delete(Workflow* workflow) {
    if (workflow) {
        scu_list_delete(workflow->rules);
        workflow->rules = nullptr;
    }
}

/**
 * @brief Parses a workflow from a specified line of text.
 *
 * The line must have the following format:
 *
 * ```plaintext
 * <workflow id>{<rule>,<rule>,...,<rule>}
 * ```
 *
 * Here, `<workflow id>` is a string of at most `MAX_ID_LENGTH - 1` characters
 * that identifies the workflow. Each `<rule>` is a string that represents a
 * rule in the workflow, and must have one of the following two formats:
 *
 * ```plaintext
 * <category><comparison><threshold>:<next workflow id>
 * <next workflow id>
 * ```
 *
 * In the first case, the rule represents a comparison rule, where `<category>`
 * is one of `'x'`, `'m'`, `'a'`, or `'s'` that indicates the category of the
 * part rating to compare against, `<comparison>` is either `<` or `>` that
 * indicates the type of comparison, `<threshold>` is the value to compare
 * against, and `<next workflow id>` is the identifier of the next workflow to
 * transition to.
 *
 * In the second case, the rule represents a forward rule, where `<next workflow
 * id>` is the identifier of the next workflow to transition to. Two special
 * cases of workflow identifiers are `'A'` and `'R'`, which indicate that the
 * workflow should immediately accept or reject the part, respectively.
 *
 * Some examples of valid lines might be the following:
 *
 * ```plaintext
 * px{a<2006:qkq,m>2090:A,rfg}
 * pv{a>1716:R,A}
 * lnx{m>1548:A,A}
 * rfg{s<537:gd,x>2440:R,A}
 * qs{s>3448:A,lnx}
 * qkq{x<1416:A,crn}
 * crn{x>2662:A,R}
 * in{s<1351:px,qqz}
 * qqz{s>2770:qs,m<1801:hdj,R}
 * gd{a>3333:R,R}
 * hdj{m>838:A,pv}
 * ```
 *
 * @param[in]  line     The line of text to parse.
 * @param[out] workflow The parsed workflow on success, otherwise unspecified.
 * @return `SCU_ERROR_NONE` on success, otherwise an appropriate error code.
 */
static ScuError workflow_parse(
    const char* restrict line,
    Workflow* restrict workflow
) {
    SCU_ASSERT(line);
    SCU_ASSERT(workflow);
    isize openBraceIndex = scu_str_index_of(line, '{');
    if ((openBraceIndex == -1) || (openBraceIndex >= MAX_ID_LENGTH)) {
        return SCU_ERROR_INVALID_FORMAT;
    }
    scu_strncpy(workflow->id, SCU_SIZEOF(workflow->id), line, openBraceIndex);
    workflow->rules = scu_list_new(Rule);
    if (!workflow->rules) {
        return SCU_ERROR_OUT_OF_MEMORY;
    }
    line += openBraceIndex + 1;
    ScuError error = SCU_ERROR_NONE;
    while (*line != '}') {
        Rule rule = { };
        if (
            ((*line == 'x') || (*line == 'm') || (*line == 'a') || (*line == 's'))
                && ((*(line + 1) == '<') || (*(line + 1) == '>'))
        ) {
            switch (*line) {
                case 'x':
                    rule.category = CATEGORY_X;
                    break;
                case 'm':
                    rule.category = CATEGORY_M;
                    break;
                case 'a':
                    rule.category = CATEGORY_A;
                    break;
                case 's':
                    rule.category = CATEGORY_S;
                    break;
                default:
                    SCU_UNREACHABLE();
            }
            line++;
            rule.type = (*line == '<') ? RULE_TYPE_CMP_LT : RULE_TYPE_CMP_GT;
            line++;
            isize read;
            if (scu_sscanf(
                    line,
                    "%" ISIZE_SCND "%" SCU_ISIZE_SCNN,
                    &rule.threshold,
                    &read
                ) != 1) {
                error = SCU_ERROR_INVALID_FORMAT;
                goto fail;
            }
            line += read;
            if (*line != ':') {
                error = SCU_ERROR_INVALID_FORMAT;
                goto fail;
            }
            line++;
            isize commaIndex = scu_str_index_of(line, ',');
            if ((commaIndex == -1) || (commaIndex >= MAX_ID_LENGTH)) {
                error = SCU_ERROR_INVALID_FORMAT;
                goto fail;
            }
            scu_strncpy(rule.next, SCU_SIZEOF(rule.next), line, commaIndex);
            line += commaIndex + 1;
        }
        else {
            isize closingBraceIndex = scu_str_index_of(line, '}');
            if (
                (closingBraceIndex == -1)
                    || (closingBraceIndex >= MAX_ID_LENGTH)
            ) {
                error = SCU_ERROR_INVALID_FORMAT;
                goto fail;
            }
            rule.type = RULE_TYPE_FWD;
            scu_strncpy(
                rule.next,
                SCU_SIZEOF(rule.next),
                line,
                closingBraceIndex
            );
            line += closingBraceIndex;
        }
        error = scu_list_add(&workflow->rules, &rule);
        if (error != SCU_ERROR_NONE) {
            goto fail;
        }
    }
    line++;
    if (*line != '\0') {
        scu_printf("%s\n", line);
        error = SCU_ERROR_INVALID_FORMAT;
        goto fail;
    }
    return SCU_ERROR_NONE;
fail:
    workflow_delete(workflow);
    return error;
}

/**
 * @brief Determines whether two specified workflows are equal.
 *
 * @warning The behavior is undefined if `a` or `b` is not a pointer to a
 * workflow.
 *
 * @param[in] a A pointer to the first workflow.
 * @param[in] b A pointer to the second workflow.
 * @return `true` if the specified workflows are equal, otherwise `false`.
 */
static bool workflow_equal(const void* a, const void* b) {
    SCU_ASSERT(a);
    SCU_ASSERT(b);
    const Workflow* l = a;
    const Workflow* r = b;
    if (scu_strcmp(l->id, r->id) != 0) {
        return false;
    }
    if (scu_list_count(l->rules) != scu_list_count(r->rules)) {
        return false;
    }
    for (isize i = 0; i < scu_list_count(l->rules); i++) {
        if (!rule_equal(&l->rules[i], &r->rules[i])) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Parses a part rating from a specified line of text.
 *
 * The line must have the following format:
 *
 * ```plaintext
 * {x=<rating>,m=<rating>,a=<rating>,s=<rating>}
 * ```
 *
 * Here, `<rating>` is an integer value that represents the rating in the
 * respective category.
 *
 * Some examples of valid lines might be the following:
 *
 * ```plaintext
 * {x=787,m=2655,a=1222,s=2876}
 * {x=1679,m=44,a=2067,s=496}
 * {x=2036,m=264,a=79,s=2244}
 * {x=2461,m=1339,a=466,s=291}
 * {x=2127,m=1623,a=2188,s=1013}
 * ```
 *
 * @param[in]  line   The line of text to parse.
 * @param[out] rating The parsed part rating on success, otherwise unspecified.
 * @return `true` if the line was successfully parsed as a part rating,
 * otherwise `false`.
 */
static bool rating_parse(const char* restrict line, Rating* restrict rating) {
    SCU_ASSERT(line);
    SCU_ASSERT(rating);
    return scu_sscanf(
        line,
        "{x=%" ISIZE_SCND ",m=%" ISIZE_SCND ",a=%" ISIZE_SCND ",s=%"
            ISIZE_SCND "}",
        &rating->x,
        &rating->m,
        &rating->a,
        &rating->s
    ) == 4;
}

/**
 * @brief Returns a hash for a specified workflow identifier.
 *
 * @warning The behavior is undefined if `value` is not a pointer to a workflow
 * identifier.
 *
 * @param[in] value A pointer to the workflow identifier to hash.
 * @return
 */
static usize workflow_id_hash(const void* value) {
    SCU_ASSERT(value);
    const char (*id)[MAX_ID_LENGTH] = value;
    const char* s = *id;
    return scu_hash_str(&s);
}

/**
 * @brief Determines whether two specified workflow identifiers are equal.
 *
 * @warning The behavior is undefined if `a` or `b` is not a pointer to a
 * workflow identifier.
 *
 * @param[in] a A pointer to the first workflow identifier.
 * @param[in] b A pointer to the second workflow identifier.
 * @return `true` if the specified workflow identifiers are equal, otherwise
 * `false`.
 */
static bool workflow_id_equal(const void* a, const void* b) {
    SCU_ASSERT(a);
    SCU_ASSERT(b);
    const char (*l)[MAX_ID_LENGTH] = a;
    const char (*r)[MAX_ID_LENGTH] = b;
    return scu_strcmp(*l, *r) == 0;
}

/**
 * @brief Parses workflows and part ratings from the standard input stream.
 *
 * The input must consist of two sections separated by an empty line. The first
 * section contains the workflows, where each line represents a workflow in the
 * format described in the documentation of `workflow_parse()`. The second
 * section contains the part ratings, where each line represents a part rating
 * in the format described in the documentation of `rating_parse()`.
 *
 * An example for a valid input might be the following:
 *
 * ```plaintext
 * px{a<2006:qkq,m>2090:A,rfg}
 * pv{a>1716:R,A}
 * lnx{m>1548:A,A}
 * rfg{s<537:gd,x>2440:R,A}
 * qs{s>3448:A,lnx}
 * qkq{x<1416:A,crn}
 * crn{x>2662:A,R}
 * in{s<1351:px,qqz}
 * qqz{s>2770:qs,m<1801:hdj,R}
 * gd{a>3333:R,R}
 * hdj{m>838:A,pv}
 *
 * {x=787,m=2655,a=1222,s=2876}
 * {x=1679,m=44,a=2067,s=496}
 * {x=2036,m=264,a=79,s=2244}
 * {x=2461,m=1339,a=466,s=291}
 * {x=2127,m=1623,a=2188,s=1013}
 * ```
 *
 * @param[out] workflows The parsed workflows on success, otherwise a `nullptr`.
 * @param[out] ratings   The parsed part ratings on success, otherwise a
 *                       `nullptr`.
 * @return `SCU_ERROR_NONE` on success, otherwise an appropriate error code.
 */
static ScuError parse_workflows_and_ratings(
    ScuHashMap** workflows,
    Rating** ratings
) {
    SCU_ASSERT(workflows);
    SCU_ASSERT(ratings);
    *workflows = scu_hash_map_new(
        char[MAX_ID_LENGTH],
        Workflow,
        workflow_id_hash,
        workflow_id_equal,
        workflow_equal
    );
    if (!*workflows) {
        *ratings = nullptr;
        return SCU_ERROR_OUT_OF_MEMORY;
    }
    ScuError error = SCU_ERROR_NONE;
    *ratings = scu_list_new(Rating);
    if (!*ratings) {
        scu_hash_map_delete(*workflows);
        *workflows = nullptr;
        return SCU_ERROR_OUT_OF_MEMORY;
    }
    char* line = nullptr;
    isize size = 0;
    while ((error = scu_readln(&line, &size)) == SCU_ERROR_NONE) {
        isize newlineIndex = scu_str_index_of(line, '\n');
        if (newlineIndex != -1) {
            line[newlineIndex] = '\0';
        }
        if (*line == '\0') {
            break;
        }
        Workflow workflow;
        error = workflow_parse(line, &workflow);
        if (error != SCU_ERROR_NONE) {
            goto fail;
        }
        error = scu_hash_map_add(*workflows, workflow.id, &workflow);
        if (error != SCU_ERROR_NONE) {
            // If we fail to add the workflow to the hash map, we need to
            // deallocate it manually here since it won't be caught by the
            // cleanup code at the end of this function.
            workflow_delete(&workflow);
            goto fail;
        }
    }
    if (error != SCU_ERROR_NONE) {
        goto fail;
    }
    while ((error = scu_readln(&line, &size)) == SCU_ERROR_NONE) {
        Rating rating;
        if (!rating_parse(line, &rating)) {
            error = SCU_ERROR_INVALID_FORMAT;
            goto fail;
        }
        error = scu_list_add(ratings, &rating);
        if (error != SCU_ERROR_NONE) {
            goto fail;
        }
    }
    if (error != SCU_ERROR_END_OF_FILE) {
        goto fail;
    }
    SCU_ARRAY_DELETE(line, size);
    return SCU_ERROR_NONE;
fail:
    SCU_ARRAY_DELETE(line, size);
    scu_list_delete(*ratings);
    *ratings = nullptr;
    scu_hash_map_delete(*workflows);
    *workflows = nullptr;
    return error;
}

/**
 * @brief Returns the sum of the part ratings of all parts that are accepted by
 * a specified hash map of workflows.
 *
 * @param[in] workflows The hash map of workflows for the computation.
 * @param[in] ratings   The list of part ratings for the computation.
 * @return The sum of the part ratings of all accepted parts.
 */
static isize sum_of_accepted_ratings(
    const ScuHashMap* workflows,
    const Rating* ratings
) {
    SCU_ASSERT(workflows);
    SCU_ASSERT(ratings);
    isize sum = 0;
    const Rating* rating;
    SCU_LIST_FOREACH(rating, ratings) {
        const char* id = "in";
        while ((scu_strcmp(id, "A") != 0) && (scu_strcmp(id, "R") != 0)) {
            const Workflow* workflow = scu_hash_map_get(workflows, id);
            const Rule* rule;
            SCU_LIST_FOREACH(rule, workflow->rules) {
                if (rule->type == RULE_TYPE_FWD) {
                    id = rule->next;
                    break;
                }
                isize value;
                switch (rule->category) {
                    case CATEGORY_X:
                        value = rating->x;
                        break;
                    case CATEGORY_M:
                        value = rating->m;
                        break;
                    case CATEGORY_A:
                        value = rating->a;
                        break;
                    case CATEGORY_S:
                        value = rating->s;
                        break;
                    default:
                        SCU_UNREACHABLE();
                }
                if (
                    ((rule->type == RULE_TYPE_CMP_LT) && (value < rule->threshold))
                        || ((rule->type == RULE_TYPE_CMP_GT) && (value > rule->threshold))
                ) {
                    id = rule->next;
                    break;
                }
            }
        }
        if (scu_strcmp(id, "A") == 0) {
            sum += rating->x + rating->m + rating->a + rating->s;
        }
    }
    return sum;
}

/**
 * @brief Returns the number of distinct ratings that are accepted by a
 * specified hash map of workflows.
 *
 * @param[in] workflows The hash map of workflows for the computation.
 * @param[in] id        The identifier of the workflow to start from.
 * @param[in] ranges    The current ranges of part ratings.
 * @return The number of distinct ratings that are accepted.
 */
static isize count_accepted_ratings(
    const ScuHashMap* workflows,
    const char* id,
    RangeSet ranges
) {
    SCU_ASSERT(workflows);
    SCU_ASSERT(id);
    if (scu_strcmp(id, "A") == 0) {
        return (ranges.x.high - ranges.x.low + 1)
            * (ranges.m.high - ranges.m.low + 1)
            * (ranges.a.high - ranges.a.low + 1)
            * (ranges.s.high - ranges.s.low + 1);
    }
    if (scu_strcmp(id, "R") == 0) {
        return 0;
    }
    const Workflow* workflow = scu_hash_map_get(workflows, id);
    isize count = 0;
    const Rule* rule;
    SCU_LIST_FOREACH(rule, workflow->rules) {
        if (rule->type == RULE_TYPE_FWD) {
            count += count_accepted_ratings(workflows, rule->next, ranges);
        }
        else {
            Range* range;
            switch (rule->category) {
                case CATEGORY_X:
                    range = &ranges.x;
                    break;
                case CATEGORY_M:
                    range = &ranges.m;
                    break;
                case CATEGORY_A:
                    range = &ranges.a;
                    break;
                case CATEGORY_S:
                    range = &ranges.s;
                    break;
                default:
                    SCU_UNREACHABLE();
            }
            Range matching = *range;
            Range remaining = *range;
            if (rule->type == RULE_TYPE_CMP_LT) {
                matching.high = rule->threshold - 1;
                remaining.low = rule->threshold;
            }
            else {
                matching.low = rule->threshold + 1;
                remaining.high = rule->threshold;
            }
            if (matching.low <= matching.high) {
                *range = matching;
                count += count_accepted_ratings(workflows, rule->next, ranges);
            }
            if (remaining.low > remaining.high) {
                break;
            }
            *range = remaining;
        }
    }
    return count;
}

/**
 * @brief Deallocates a specified hash map of workflows.
 *
 * @note If `workflows` is a `nullptr`, this function does nothing.
 *
 * @warning The behavior is undefined if `workflows` is used after it has been
 * deallocated.
 *
 * @param[in, out] workflows The hash map of workflows to deallocate.
 */
static inline void workflows_delete(ScuHashMap* workflows) {
    if (workflows) {
        ScuHashMapEntry entry;
        SCU_HASH_MAP_FOREACH(entry, workflows) {
            workflow_delete(entry.value);
        }
        scu_hash_map_delete(workflows);
    }
}

/**
 * @brief Deallocates a specified list of part ratings.
 *
 * @note If `ratings` is a `nullptr`, this function does nothing.
 *
 * @warning The behavior is undefined if `ratings` is used after it has been
 * deallocated.
 *
 * @param[in, out] ratings The list of part ratings to deallocate.
 */
static inline void ratings_delete(Rating* ratings) {
    if (ratings) {
        scu_list_delete(ratings);
    }
}

int main() {
    ScuHashMap* workflows;
    Rating* ratings;
    ScuError error = parse_workflows_and_ratings(&workflows, &ratings);
    if (error != SCU_ERROR_NONE) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while reading the input file (code %d).\n",
            error
        );
        return EXIT_FAILURE;
    }
    isize sum = sum_of_accepted_ratings(workflows, ratings);
    isize count = count_accepted_ratings(
        workflows,
        "in",
        (RangeSet) {
            .x = { .low = RATING_MIN, .high = RATING_MAX },
            .m = { .low = RATING_MIN, .high = RATING_MAX },
            .a = { .low = RATING_MIN, .high = RATING_MAX },
            .s = { .low = RATING_MIN, .high = RATING_MAX }
        }
    );
    scu_printf(
        "The sum of the part ratings of all accepted parts is %" ISIZE_PRID
            ".\n",
        sum
    );
    scu_printf(
        "The number of distinct ratings that are accepted is %" ISIZE_PRID
            ".\n",
        count
    );
    workflows_delete(workflows);
    ratings_delete(ratings);
    return EXIT_SUCCESS;
}