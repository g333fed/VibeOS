/*
 * VibeOS Web Browser
 *
 * A simple text-mode web browser with basic HTML rendering.
 *
 * Usage: browser [url]
 * Example: browser http://example.com/
 */

#include "../lib/vibe.h"
#include "../lib/gfx.h"

static kapi_t *k;

// ============ String Helpers ============

static int str_len(const char *s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

static int str_eqn(const char *a, const char *b, int n) {
    while (n > 0 && *a && *b && *a == *b) { a++; b++; n--; }
    return n == 0;
}

static int str_ieqn(const char *a, const char *b, int n) {
    while (n > 0 && *a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return 0;
        a++; b++; n--;
    }
    return n == 0;
}

static void str_cpy(char *dst, const char *src) {
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static void str_ncpy(char *dst, const char *src, int n) {
    while (n > 0 && *src) { *dst++ = *src++; n--; }
    *dst = '\0';
}

static int parse_int(const char *s) {
    int n = 0;
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    return n;
}

// ============ URL Parsing ============

typedef struct {
    char host[256];
    char path[512];
    int port;
    int use_tls;  // 1 for https, 0 for http
} url_t;

static int parse_url(const char *url, url_t *out) {
    out->use_tls = 0;
    out->port = 80;

    // Check for https://
    if (str_eqn(url, "https://", 8)) {
        url += 8;
        out->use_tls = 1;
        out->port = 443;
    } else if (str_eqn(url, "http://", 7)) {
        url += 7;
    }

    const char *host_start = url;
    const char *host_end = url;
    while (*host_end && *host_end != '/' && *host_end != ':') host_end++;

    int host_len = host_end - host_start;
    if (host_len >= 256) return -1;
    str_ncpy(out->host, host_start, host_len);

    // Parse port if present
    if (*host_end == ':') {
        host_end++;
        out->port = parse_int(host_end);
        while (*host_end >= '0' && *host_end <= '9') host_end++;
    }

    if (*host_end == '/') str_cpy(out->path, host_end);
    else str_cpy(out->path, "/");

    return 0;
}

// ============ HTTP Client ============

typedef struct {
    int status_code;
    int content_length;
    char location[512];
    int header_len;
} http_response_t;

static int find_header_end(const char *buf, int len) {
    for (int i = 0; i < len - 3; i++) {
        if (buf[i] == '\r' && buf[i+1] == '\n' &&
            buf[i+2] == '\r' && buf[i+3] == '\n') {
            return i + 4;
        }
    }
    return -1;
}

static int parse_headers(const char *buf, int len, http_response_t *resp) {
    memset(resp, 0, sizeof(*resp));
    resp->content_length = -1;
    resp->header_len = find_header_end(buf, len);
    if (resp->header_len < 0) return -1;

    const char *p = buf;
    if (!str_eqn(p, "HTTP/1.", 7)) return -1;
    p += 7;
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;
    resp->status_code = parse_int(p);

    while (*p && *p != '\r') p++;
    if (*p == '\r') p += 2;

    while (p < buf + resp->header_len - 2) {
        const char *line_end = p;
        while (line_end < buf + resp->header_len && *line_end != '\r') line_end++;

        if (str_ieqn(p, "Content-Length:", 15)) {
            const char *val = p + 15;
            while (*val == ' ') val++;
            resp->content_length = parse_int(val);
        } else if (str_ieqn(p, "Location:", 9)) {
            const char *val = p + 9;
            while (*val == ' ') val++;
            int loc_len = line_end - val;
            if (loc_len >= 512) loc_len = 511;
            str_ncpy(resp->location, val, loc_len);
        }
        p = line_end + 2;
    }
    return 0;
}

static int http_get(url_t *url, char *response, int max_response, http_response_t *resp) {
    uint32_t ip = k->dns_resolve(url->host);
    if (ip == 0) return -1;

    // Connect (TLS or plain TCP)
    int sock;
    if (url->use_tls) {
        sock = k->tls_connect(ip, url->port, url->host);
    } else {
        sock = k->tcp_connect(ip, url->port);
    }
    if (sock < 0) return -1;

    char request[1024];
    char *p = request;
    const char *s;

    s = "GET "; while (*s) *p++ = *s++;
    s = url->path; while (*s) *p++ = *s++;
    s = " HTTP/1.0\r\nHost: "; while (*s) *p++ = *s++;
    s = url->host; while (*s) *p++ = *s++;
    s = "\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36\r\nAccept: text/html,*/*\r\nConnection: close\r\n\r\n";
    while (*s) *p++ = *s++;
    *p = '\0';

    // Send request
    int sent;
    if (url->use_tls) {
        sent = k->tls_send(sock, request, p - request);
    } else {
        sent = k->tcp_send(sock, request, p - request);
    }
    if (sent < 0) {
        if (url->use_tls) k->tls_close(sock);
        else k->tcp_close(sock);
        return -1;
    }

    int total = 0;
    int timeout = 0;
    resp->header_len = 0;

    while (total < max_response - 1 && timeout < 500) {
        int n;
        if (url->use_tls) {
            n = k->tls_recv(sock, response + total, max_response - 1 - total);
        } else {
            n = k->tcp_recv(sock, response + total, max_response - 1 - total);
        }
        if (n < 0) break;  // Connection closed
        if (n == 0) {
            k->net_poll();
            k->sleep_ms(10);
            timeout++;
            continue;
        }
        total += n;
        timeout = 0;

        // Check if we got headers yet
        if (resp->header_len == 0) {
            response[total] = '\0';
            parse_headers(response, total, resp);

            // If we have Content-Length and got all content, we're done
            if (resp->header_len > 0 && resp->content_length >= 0) {
                int body_received = total - resp->header_len;
                if (body_received >= resp->content_length) break;
            }
        }
    }

    response[total] = '\0';
    if (url->use_tls) k->tls_close(sock);
    else k->tcp_close(sock);
    if (resp->header_len == 0) parse_headers(response, total, resp);
    return total;
}

static int is_redirect(int status) {
    return status == 301 || status == 302 || status == 307 || status == 308;
}

// ============ HTML Parser ============

// Parsed content - simple linked list of text blocks
typedef struct text_block {
    char *text;
    char *link_url;      // URL if this is a link
    int is_heading;      // h1-h6
    int is_bold;
    int is_italic;
    int is_link;
    int is_list_item;    // -1 for unordered, or item number for ordered (1, 2, 3...), 0 for none
    int is_paragraph;
    int is_preformatted; // <pre> or <code>
    int is_blockquote;
    int is_image;        // placeholder for <img>
    int is_newline;      // This block forces a new line before it
    struct text_block *next;
} text_block_t;

// Clickable link regions for hit testing
typedef struct {
    int x, y, w, h;      // Bounding box (relative to content, not scroll)
    char url[512];
} link_region_t;

#define MAX_LINK_REGIONS 256
static link_region_t link_regions[MAX_LINK_REGIONS];
static int num_link_regions = 0;

// Current link URL being parsed
static char current_link_url[512] = "";

static text_block_t *blocks_head = NULL;
static text_block_t *blocks_tail = NULL;

// Style state for parsing
typedef struct {
    int heading;      // h1-h6 level (0 = none)
    int bold;
    int italic;
    int link;
    int list_item;
    int preformatted;
    int blockquote;
} style_state_t;

static void add_block_styled(const char *text, int len, style_state_t *style, int is_para, int is_image) {
    if (len <= 0) return;

    // Skip pure whitespace blocks (unless preformatted)
    if (!style->preformatted) {
        int has_content = 0;
        for (int i = 0; i < len; i++) {
            if (text[i] != ' ' && text[i] != '\t' && text[i] != '\n' && text[i] != '\r') {
                has_content = 1;
                break;
            }
        }
        if (!has_content) return;
    }

    text_block_t *block = k->malloc(sizeof(text_block_t));
    if (!block) return;

    block->text = k->malloc(len + 1);
    if (!block->text) { k->free(block); return; }

    // Copy text - normalize whitespace unless preformatted
    char *dst = block->text;
    if (style->preformatted) {
        // Keep whitespace intact for <pre>/<code>
        for (int i = 0; i < len; i++) {
            *dst++ = text[i];
        }
    } else {
        int last_was_space = 1;
        for (int i = 0; i < len; i++) {
            char c = text[i];
            if (c == '\n' || c == '\r' || c == '\t') c = ' ';
            if (c == ' ' && last_was_space) continue;
            *dst++ = c;
            last_was_space = (c == ' ');
        }
        // Trim trailing space
        if (dst > block->text && *(dst-1) == ' ') dst--;
    }
    *dst = '\0';

    if (dst == block->text && !is_image) {
        k->free(block->text);
        k->free(block);
        return;
    }

    block->is_heading = style->heading;
    block->is_bold = style->bold;
    block->is_italic = style->italic;
    block->is_link = style->link;
    block->is_list_item = style->list_item;
    block->is_paragraph = is_para;
    block->is_preformatted = style->preformatted;
    block->is_blockquote = style->blockquote;
    block->is_image = is_image;
    block->is_newline = 0;  // Default: continue on same line
    block->next = NULL;

    // Store link URL if this is a link
    block->link_url = NULL;
    if (style->link && current_link_url[0]) {
        int url_len = str_len(current_link_url);
        block->link_url = k->malloc(url_len + 1);
        if (block->link_url) {
            str_cpy(block->link_url, current_link_url);
        }
    }

    if (blocks_tail) {
        blocks_tail->next = block;
        blocks_tail = block;
    } else {
        blocks_head = blocks_tail = block;
    }
}

// Backwards-compatible wrapper for simple calls
static void add_block(const char *text, int len, int heading, int bold, int link, int list_item, int para) {
    style_state_t style = {0};
    style.heading = heading;
    style.bold = bold;
    style.link = link;
    style.list_item = list_item;
    add_block_styled(text, len, &style, para, 0);
}

// Add a newline marker - next block starts on new line
static void add_newline(void) {
    // Mark the next block (or create empty marker) to start on new line
    // We do this by creating a minimal block with is_newline set
    text_block_t *block = k->malloc(sizeof(text_block_t));
    if (!block) return;
    block->text = NULL;
    block->link_url = NULL;
    block->is_heading = 0;
    block->is_bold = 0;
    block->is_italic = 0;
    block->is_link = 0;
    block->is_list_item = 0;
    block->is_paragraph = 0;
    block->is_preformatted = 0;
    block->is_blockquote = 0;
    block->is_image = 0;
    block->is_newline = 1;
    block->next = NULL;

    if (blocks_tail) {
        blocks_tail->next = block;
        blocks_tail = block;
    } else {
        blocks_head = blocks_tail = block;
    }
}

static void free_blocks(void) {
    text_block_t *b = blocks_head;
    while (b) {
        text_block_t *next = b->next;
        if (b->text) k->free(b->text);
        if (b->link_url) k->free(b->link_url);
        k->free(b);
        b = next;
    }
    blocks_head = blocks_tail = NULL;
    num_link_regions = 0;
}

// Helper to extract an attribute value from tag
static int extract_attr(const char *attrs_start, const char *tag_end, const char *attr_name, char *out, int max_len) {
    int attr_name_len = str_len(attr_name);
    const char *ap = attrs_start;
    out[0] = '\0';

    while (ap < tag_end) {
        // Skip whitespace
        while (ap < tag_end && (*ap == ' ' || *ap == '\t' || *ap == '\n')) ap++;
        if (ap >= tag_end) break;

        // Check for the attribute
        if (str_ieqn(ap, attr_name, attr_name_len) && (ap[attr_name_len] == '=' || ap[attr_name_len] == ' ')) {
            ap += attr_name_len;
            while (ap < tag_end && *ap == ' ') ap++;
            if (*ap == '=') {
                ap++;
                while (ap < tag_end && *ap == ' ') ap++;
                char quote = 0;
                if (*ap == '"' || *ap == '\'') {
                    quote = *ap++;
                }
                const char *val_start = ap;
                if (quote) {
                    while (ap < tag_end && *ap != quote) ap++;
                } else {
                    while (ap < tag_end && *ap != '>' && *ap != ' ') ap++;
                }
                int val_len = ap - val_start;
                if (val_len > 0 && val_len < max_len) {
                    str_ncpy(out, val_start, val_len);
                    return val_len;
                }
            }
        }
        // Skip to next attribute
        while (ap < tag_end && *ap != ' ' && *ap != '\t') ap++;
    }
    return 0;
}

// Simple HTML parser
static void parse_html(const char *html, int len) {
    free_blocks();

    const char *p = html;
    const char *end = html + len;

    int in_head = 0;
    int in_script = 0;
    int in_style = 0;

    // Use style state for all formatting
    style_state_t style = {0};

    // Track list state for ordered lists
    int in_ordered_list = 0;
    int list_item_number = 0;

    const char *text_start = NULL;

    while (p < end) {
        if (*p == '<') {
            // Check for comment first (before any flush)
            if (p + 3 < end && p[1] == '!' && p[2] == '-' && p[3] == '-') {
                // Flush before comment
                if (text_start && !in_head && !in_script && !in_style) {
                    add_block_styled(text_start, p - text_start, &style, 0, 0);
                }
                text_start = NULL;
                // Skip HTML comment
                p += 4;
                while (p + 2 < end && !(p[0] == '-' && p[1] == '-' && p[2] == '>')) p++;
                if (p + 2 < end) p += 3;
                continue;
            }

            // Flush pending text
            if (text_start && !in_head && !in_script && !in_style) {
                add_block_styled(text_start, p - text_start, &style, 0, 0);
            }
            text_start = NULL;

            // Parse tag
            p++;
            int closing = (*p == '/');
            if (closing) p++;

            const char *tag_start = p;
            while (p < end && *p != '>' && *p != ' ' && *p != '\t' && *p != '\n' && *p != '/') p++;
            int tag_len = p - tag_start;

            // Save position after tag name for attribute parsing
            const char *attrs_start = p;

            // Skip to end of tag
            while (p < end && *p != '>') p++;
            const char *tag_end = p;
            if (p < end) p++;

            // Handle tags
            if (str_ieqn(tag_start, "head", 4) && tag_len == 4) {
                in_head = !closing;
            } else if (str_ieqn(tag_start, "script", 6) && tag_len == 6) {
                in_script = !closing;
            } else if (str_ieqn(tag_start, "style", 5) && tag_len == 5) {
                in_style = !closing;
            } else if ((tag_start[0] == 'h' || tag_start[0] == 'H') && tag_len == 2 &&
                       tag_start[1] >= '1' && tag_start[1] <= '6') {
                style.heading = closing ? 0 : (tag_start[1] - '0');
                if (closing) add_newline();
            } else if ((str_ieqn(tag_start, "b", 1) && tag_len == 1) ||
                       (str_ieqn(tag_start, "strong", 6) && tag_len == 6)) {
                style.bold = !closing;
            } else if ((str_ieqn(tag_start, "i", 1) && tag_len == 1) ||
                       (str_ieqn(tag_start, "em", 2) && tag_len == 2)) {
                style.italic = !closing;
            } else if (str_ieqn(tag_start, "a", 1) && tag_len == 1) {
                if (closing) {
                    style.link = 0;
                    current_link_url[0] = '\0';
                } else {
                    style.link = 1;
                    extract_attr(attrs_start, tag_end, "href", current_link_url, 511);
                }
            } else if (str_ieqn(tag_start, "li", 2) && tag_len == 2) {
                if (!closing) {
                    // Add newline before list item, then bullet/number will be rendered by draw code
                    add_newline();
                    if (in_ordered_list) {
                        list_item_number++;
                        style.list_item = list_item_number;  // Store item number (1, 2, 3...)
                    } else {
                        style.list_item = -1;  // -1 means unordered (bullet)
                    }
                } else {
                    style.list_item = 0;
                }
            } else if ((str_ieqn(tag_start, "pre", 3) && tag_len == 3)) {
                if (!closing) {
                    add_newline();
                    style.preformatted = 1;
                } else {
                    style.preformatted = 0;
                    add_newline();
                }
            } else if (str_ieqn(tag_start, "code", 4) && tag_len == 4) {
                // Inline code - just mark as preformatted if not inside <pre>
                if (!style.preformatted) {
                    // We could add special styling later
                }
            } else if (str_ieqn(tag_start, "blockquote", 10) && tag_len == 10) {
                if (!closing) {
                    add_newline();
                    style.blockquote = 1;
                } else {
                    style.blockquote = 0;
                    add_newline();
                }
            } else if (str_ieqn(tag_start, "img", 3) && tag_len == 3) {
                // Image placeholder
                char alt[128] = "";
                extract_attr(attrs_start, tag_end, "alt", alt, 127);

                // Create placeholder text
                char placeholder[160];
                if (alt[0]) {
                    char *d = placeholder;
                    const char *s = "[IMG: ";
                    while (*s) *d++ = *s++;
                    s = alt;
                    while (*s && d < placeholder + 150) *d++ = *s++;
                    *d++ = ']';
                    *d = '\0';
                } else {
                    str_cpy(placeholder, "[IMG]");
                }
                add_block_styled(placeholder, str_len(placeholder), &style, 0, 1);
            } else if ((str_ieqn(tag_start, "p", 1) && tag_len == 1) ||
                       (str_ieqn(tag_start, "div", 3) && tag_len == 3) ||
                       (str_ieqn(tag_start, "br", 2) && (tag_len == 2 || tag_len == 3)) ||
                       (str_ieqn(tag_start, "hr", 2) && tag_len == 2)) {
                add_newline();
                if (str_ieqn(tag_start, "hr", 2)) {
                    add_block("----------------------------------------", 40, 0, 0, 0, 0, 0);
                    add_newline();
                }
            } else if (str_ieqn(tag_start, "ul", 2) && tag_len == 2) {
                add_newline();
                if (!closing) {
                    in_ordered_list = 0;
                    list_item_number = 0;
                }
            } else if (str_ieqn(tag_start, "ol", 2) && tag_len == 2) {
                add_newline();
                if (!closing) {
                    in_ordered_list = 1;
                    list_item_number = 0;
                } else {
                    in_ordered_list = 0;
                }
            } else if (str_ieqn(tag_start, "tr", 2) && tag_len == 2) {
                // Table row - add newline
                if (closing) add_newline();
            } else if (str_ieqn(tag_start, "td", 2) && tag_len == 2) {
                // Table cell - add tab separator
                if (closing) add_block(" | ", 3, 0, 0, 0, 0, 0);
            } else if (str_ieqn(tag_start, "th", 2) && tag_len == 2) {
                // Table header - bold and tab separator
                style.bold = !closing;
                if (closing) add_block(" | ", 3, 0, 0, 0, 0, 0);
            } else if (str_ieqn(tag_start, "table", 5) && tag_len == 5) {
                add_newline();
            } else if (str_ieqn(tag_start, "title", 5) && tag_len == 5) {
                // Skip title text
                in_head = !closing;
            } else if (str_ieqn(tag_start, "span", 4) && tag_len == 4) {
                // Span - ignore, just container
            } else if (str_ieqn(tag_start, "sup", 3) && tag_len == 3) {
                // Superscript - show in brackets
                if (!closing) add_block("^", 1, 0, 0, 0, 0, 0);
            } else if (str_ieqn(tag_start, "sub", 3) && tag_len == 3) {
                // Subscript - show in brackets
                if (!closing) add_block("_", 1, 0, 0, 0, 0, 0);
            }
        } else if (*p == '&') {
            // HTML entity
            if (text_start && !in_head && !in_script && !in_style) {
                add_block_styled(text_start, p - text_start, &style, 0, 0);
            }

            const char *entity_start = p;
            while (p < end && *p != ';' && *p != ' ' && *p != '<') p++;

            // Decode HTML entities
            char decoded[8] = {0};
            int decoded_len = 0;

            if (str_eqn(entity_start, "&amp;", 5)) {
                decoded[0] = '&'; decoded_len = 1;
            } else if (str_eqn(entity_start, "&lt;", 4)) {
                decoded[0] = '<'; decoded_len = 1;
            } else if (str_eqn(entity_start, "&gt;", 4)) {
                decoded[0] = '>'; decoded_len = 1;
            } else if (str_eqn(entity_start, "&quot;", 6)) {
                decoded[0] = '"'; decoded_len = 1;
            } else if (str_eqn(entity_start, "&apos;", 6)) {
                decoded[0] = '\''; decoded_len = 1;
            } else if (str_eqn(entity_start, "&nbsp;", 6)) {
                decoded[0] = ' '; decoded_len = 1;
            } else if (str_eqn(entity_start, "&copy;", 6)) {
                decoded[0] = '('; decoded[1] = 'c'; decoded[2] = ')'; decoded_len = 3;
            } else if (str_eqn(entity_start, "&reg;", 5)) {
                decoded[0] = '('; decoded[1] = 'R'; decoded[2] = ')'; decoded_len = 3;
            } else if (str_eqn(entity_start, "&trade;", 7)) {
                decoded[0] = 'T'; decoded[1] = 'M'; decoded_len = 2;
            } else if (str_eqn(entity_start, "&mdash;", 7) || str_eqn(entity_start, "&#8212;", 7)) {
                decoded[0] = '-'; decoded[1] = '-'; decoded_len = 2;
            } else if (str_eqn(entity_start, "&ndash;", 7) || str_eqn(entity_start, "&#8211;", 7)) {
                decoded[0] = '-'; decoded_len = 1;
            } else if (str_eqn(entity_start, "&bull;", 6) || str_eqn(entity_start, "&#8226;", 7)) {
                decoded[0] = '*'; decoded_len = 1;
            } else if (str_eqn(entity_start, "&hellip;", 8) || str_eqn(entity_start, "&#8230;", 7)) {
                decoded[0] = '.'; decoded[1] = '.'; decoded[2] = '.'; decoded_len = 3;
            } else if (str_eqn(entity_start, "&laquo;", 7)) {
                decoded[0] = '<'; decoded[1] = '<'; decoded_len = 2;
            } else if (str_eqn(entity_start, "&raquo;", 7)) {
                decoded[0] = '>'; decoded[1] = '>'; decoded_len = 2;
            } else if (str_eqn(entity_start, "&ldquo;", 7) || str_eqn(entity_start, "&rdquo;", 7) ||
                       str_eqn(entity_start, "&#8220;", 7) || str_eqn(entity_start, "&#8221;", 7)) {
                decoded[0] = '"'; decoded_len = 1;
            } else if (str_eqn(entity_start, "&lsquo;", 7) || str_eqn(entity_start, "&rsquo;", 7) ||
                       str_eqn(entity_start, "&#8216;", 7) || str_eqn(entity_start, "&#8217;", 7)) {
                decoded[0] = '\''; decoded_len = 1;
            } else if (str_eqn(entity_start, "&pound;", 7)) {
                decoded[0] = 'L'; decoded_len = 1;  // £ -> L
            } else if (str_eqn(entity_start, "&euro;", 6)) {
                decoded[0] = 'E'; decoded_len = 1;  // € -> E
            } else if (str_eqn(entity_start, "&yen;", 5)) {
                decoded[0] = 'Y'; decoded_len = 1;  // ¥ -> Y
            } else if (str_eqn(entity_start, "&cent;", 6)) {
                decoded[0] = 'c'; decoded_len = 1;
            } else if (str_eqn(entity_start, "&deg;", 5)) {
                decoded[0] = 'o'; decoded_len = 1;  // ° -> o
            } else if (str_eqn(entity_start, "&plusmn;", 8)) {
                decoded[0] = '+'; decoded[1] = '/'; decoded[2] = '-'; decoded_len = 3;
            } else if (str_eqn(entity_start, "&times;", 7)) {
                decoded[0] = 'x'; decoded_len = 1;
            } else if (str_eqn(entity_start, "&divide;", 8)) {
                decoded[0] = '/'; decoded_len = 1;
            } else if (entity_start[1] == '#') {
                // Numeric entity &#123; or &#x1F;
                int val = 0;
                const char *np = entity_start + 2;
                if (*np == 'x' || *np == 'X') {
                    // Hex
                    np++;
                    while (*np != ';' && np < p) {
                        char c = *np++;
                        if (c >= '0' && c <= '9') val = val * 16 + (c - '0');
                        else if (c >= 'a' && c <= 'f') val = val * 16 + (c - 'a' + 10);
                        else if (c >= 'A' && c <= 'F') val = val * 16 + (c - 'A' + 10);
                    }
                } else {
                    // Decimal
                    while (*np != ';' && np < p) {
                        if (*np >= '0' && *np <= '9') val = val * 10 + (*np - '0');
                        np++;
                    }
                }
                // Convert to ASCII if possible, otherwise show placeholder
                if (val >= 32 && val < 127) {
                    decoded[0] = (char)val; decoded_len = 1;
                } else if (val == 160) {  // non-breaking space
                    decoded[0] = ' '; decoded_len = 1;
                } else if (val == 8212) {  // em-dash
                    decoded[0] = '-'; decoded[1] = '-'; decoded_len = 2;
                } else if (val == 8211) {  // en-dash
                    decoded[0] = '-'; decoded_len = 1;
                } else if (val == 8217 || val == 8216) {  // smart quotes
                    decoded[0] = '\''; decoded_len = 1;
                } else if (val == 8221 || val == 8220) {
                    decoded[0] = '"'; decoded_len = 1;
                }
                // else: skip unknown unicode
            }

            if (decoded_len > 0) {
                add_block_styled(decoded, decoded_len, &style, 0, 0);
            }

            if (*p == ';') p++;
            text_start = p;
        } else {
            if (!text_start) text_start = p;
            p++;
        }
    }

    // Flush remaining text
    if (text_start && !in_head && !in_script && !in_style) {
        add_block_styled(text_start, p - text_start, &style, 0, 0);
    }
}

// ============ Browser UI ============

#define WIN_WIDTH 600
#define WIN_HEIGHT 400
#define ADDR_BAR_HEIGHT 24
#define CONTENT_Y (ADDR_BAR_HEIGHT + 2)
#define CHAR_W 8
#define CHAR_H 16
#define MARGIN 8

static int window_id = -1;
static uint32_t *win_buf = NULL;
static int win_w, win_h;
static char current_url[512] = "";
static int scroll_offset = 0;
static int content_height = 0;
static int editing_url = 0;
static char url_input[512] = "";
static int cursor_pos = 0;
static int dragging_scrollbar = 0;
static int drag_start_y = 0;
static int drag_start_scroll = 0;

// History for back button
#define MAX_HISTORY 32
static char history[MAX_HISTORY][512];
static int history_pos = -1;
static int history_len = 0;

// Graphics context
static gfx_ctx_t gfx;

// Scrollbar dimensions (calculated in draw)
static int scrollbar_y = 0;
static int scrollbar_h = 0;
#define SCROLLBAR_W 12

static void draw_browser(void) {
    if (!win_buf) return;

    // Clear link regions
    num_link_regions = 0;

    // Clear
    gfx_fill_rect(&gfx, 0, 0, win_w, win_h, COLOR_WHITE);

    // Address bar background
    gfx_fill_rect(&gfx, 0, 0, win_w, ADDR_BAR_HEIGHT, 0x00DDDDDD);
    gfx_draw_rect(&gfx, 0, ADDR_BAR_HEIGHT - 1, win_w, 1, COLOR_BLACK);

    // Back button
    #define BACK_BTN_W 24
    uint32_t back_color = (history_pos > 0) ? COLOR_BLACK : 0x00888888;
    gfx_fill_rect(&gfx, 4, 4, BACK_BTN_W, 16, 0x00EEEEEE);
    gfx_draw_rect(&gfx, 4, 4, BACK_BTN_W, 16, back_color);
    gfx_draw_string(&gfx, 8, 4, "<", back_color, 0x00EEEEEE);

    // URL input box (shifted right for back button)
    int url_x = 4 + BACK_BTN_W + 4;
    gfx_fill_rect(&gfx, url_x, 4, win_w - url_x - 4, 16, COLOR_WHITE);
    gfx_draw_rect(&gfx, url_x, 4, win_w - url_x - 4, 16, COLOR_BLACK);

    // URL text
    const char *display_url = editing_url ? url_input : current_url;
    gfx_draw_string(&gfx, url_x + 4, 4, display_url, COLOR_BLACK, COLOR_WHITE);

    // Cursor when editing
    if (editing_url) {
        int cursor_x = url_x + 4 + cursor_pos * CHAR_W;
        gfx_fill_rect(&gfx, cursor_x, 5, 1, 14, COLOR_BLACK);
    }

    // Content area
    int y = CONTENT_Y + MARGIN - scroll_offset;
    int base_margin = MARGIN;
    int max_chars = (win_w - MARGIN * 2 - SCROLLBAR_W) / CHAR_W;
    int current_x = base_margin;  // Track horizontal position across inline blocks

    text_block_t *block = blocks_head;
    while (block) {
        if (y > win_h) break;

        // Handle newline blocks
        if (block->is_newline) {
            y += CHAR_H;
            current_x = base_margin;
            block = block->next;
            continue;
        }

        // Skip empty blocks
        if (!block->text) {
            block = block->next;
            continue;
        }

        const char *text = block->text;
        int len = str_len(text);

        // Adjust margin for blockquotes and list items
        int left_margin = base_margin;
        if (block->is_blockquote) {
            left_margin += CHAR_W * 2;  // Indent blockquotes
        }
        if (block->is_list_item) {
            // Ordered lists need more space for "10." etc
            left_margin += CHAR_W * 3;  // Indent list items (bullet/number goes in the margin)
        }

        // Adjust max chars for blockquote/list
        int line_max = max_chars;
        if (block->is_blockquote) line_max -= 2;
        if (block->is_list_item) line_max -= 3;

        // Track if we're on first line of block (for bullet rendering)
        int first_line = 1;

        // For preformatted text, don't word wrap
        int do_word_wrap = !block->is_preformatted;

        // Word wrap (or not for preformatted)
        int pos = 0;
        while (pos < len) {
            // Find line break point
            int line_len = 0;
            int last_space = -1;

            if (block->is_preformatted) {
                // For preformatted, break only at newlines
                while (pos + line_len < len && text[pos + line_len] != '\n') {
                    line_len++;
                }
            } else {
                while (pos + line_len < len && line_len < line_max) {
                    if (text[pos + line_len] == '\n') break;
                    if (text[pos + line_len] == ' ') last_space = line_len;
                    line_len++;
                }

                // Break at word boundary if possible
                if (do_word_wrap && pos + line_len < len && last_space > 0 && line_len >= line_max) {
                    line_len = last_space + 1;
                }
            }

            // Draw line if visible
            if (y + CHAR_H > CONTENT_Y && y < win_h - 16) {
                // Determine foreground color
                uint32_t fg = COLOR_BLACK;
                uint32_t bg = COLOR_WHITE;

                if (block->is_link) {
                    fg = 0x000000FF;  // Blue for links
                } else if (block->is_image) {
                    fg = 0x00666666;  // Gray for image placeholders
                    bg = 0x00EEEEEE;  // Light gray background
                } else if (block->is_preformatted) {
                    bg = 0x00F0F0F0;  // Slight gray background for code
                }

                // Draw blockquote indicator
                if (block->is_blockquote) {
                    gfx_fill_rect(&gfx, base_margin, y, 3, CHAR_H, 0x00888888);
                }

                // Draw list bullet or number on first line
                if (block->is_list_item && first_line) {
                    if (block->is_list_item == -1) {
                        // Unordered list - bullet
                        gfx_draw_char(&gfx, base_margin, y, '*', COLOR_BLACK, COLOR_WHITE);
                    } else {
                        // Ordered list - number
                        int num = block->is_list_item;
                        char num_buf[8];
                        int i = 0;
                        // Convert number to string (reversed)
                        do {
                            num_buf[i++] = '0' + (num % 10);
                            num /= 10;
                        } while (num > 0);
                        // Draw in correct order
                        int nx = base_margin;
                        while (i > 0) {
                            gfx_draw_char(&gfx, nx, y, num_buf[--i], COLOR_BLACK, COLOR_WHITE);
                            nx += CHAR_W;
                        }
                        gfx_draw_char(&gfx, nx, y, '.', COLOR_BLACK, COLOR_WHITE);
                    }
                }

                // Draw background for special blocks
                if (block->is_image || block->is_preformatted) {
                    int line_width = 0;
                    for (int i = 0; i < line_len && text[pos + i] != '\n'; i++) {
                        line_width++;
                    }
                    gfx_fill_rect(&gfx, left_margin - 2, y, line_width * CHAR_W + 4, CHAR_H, bg);
                }

                // Use current_x for inline continuation, left_margin for block start
                int start_x;
                if (current_x > left_margin) {
                    // Continuing on same line - add space between blocks
                    start_x = current_x + CHAR_W;
                } else {
                    start_x = left_margin;
                }

                // Draw character by character for styling
                int actual_chars = 0;
                int x = start_x;
                for (int i = 0; i < line_len && text[pos + i] != '\n'; i++) {
                    char c = text[pos + i];
                    if (x + CHAR_W > win_w - SCROLLBAR_W - MARGIN) {
                        // Wrap to next line
                        y += CHAR_H;
                        x = left_margin;
                        current_x = left_margin;
                    }
                    gfx_draw_char(&gfx, x, y, c, fg, bg);
                    x += CHAR_W;
                    actual_chars++;
                }
                current_x = x;  // Save position for next inline block

                // Underline for links
                if (block->is_link) {
                    gfx_fill_rect(&gfx, start_x, y + CHAR_H - 2,
                                  actual_chars * CHAR_W, 1, fg);
                }

                // Register link region for hit testing
                if (block->is_link && block->link_url && num_link_regions < MAX_LINK_REGIONS && actual_chars > 0) {
                    link_region_t *lr = &link_regions[num_link_regions++];
                    lr->x = start_x;
                    lr->y = y;
                    lr->w = actual_chars * CHAR_W;
                    lr->h = CHAR_H;
                    str_ncpy(lr->url, block->link_url, 511);
                }

                // Underline for h1 headings
                if (block->is_heading == 1) {
                    gfx_fill_rect(&gfx, left_margin, y + CHAR_H - 2,
                                  actual_chars * CHAR_W, 2, COLOR_BLACK);
                }

                // Draw image box border
                if (block->is_image) {
                    gfx_draw_rect(&gfx, left_margin - 3, y - 1,
                                  actual_chars * CHAR_W + 6, CHAR_H + 2, 0x00888888);
                }
            }

            pos += line_len;
            first_line = 0;

            // Skip newline in text and advance y
            if (pos < len && text[pos] == '\n') {
                pos++;
                y += CHAR_H;
                current_x = left_margin;
            }
        }

        // Extra space after paragraphs and special blocks
        if (block->is_paragraph || block->is_heading || block->is_blockquote || block->is_image) {
            y += CHAR_H / 2;
            current_x = base_margin;
        }

        block = block->next;
    }

    content_height = y + scroll_offset - CONTENT_Y;

    // Scrollbar if needed
    if (content_height > win_h - CONTENT_Y) {
        int content_area = win_h - CONTENT_Y - 16;  // minus status bar
        scrollbar_h = content_area * content_area / content_height;
        if (scrollbar_h < 20) scrollbar_h = 20;
        int max_scroll = content_height - content_area;
        if (max_scroll > 0) {
            scrollbar_y = CONTENT_Y + scroll_offset * (content_area - scrollbar_h) / max_scroll;
        } else {
            scrollbar_y = CONTENT_Y;
        }
        // Draw scrollbar track
        gfx_fill_rect(&gfx, win_w - SCROLLBAR_W, CONTENT_Y, SCROLLBAR_W, content_area, 0x00CCCCCC);
        // Draw scrollbar thumb
        gfx_fill_rect(&gfx, win_w - SCROLLBAR_W + 2, scrollbar_y, SCROLLBAR_W - 4, scrollbar_h, 0x00666666);
    } else {
        scrollbar_h = 0;
    }

    // Status bar
    gfx_fill_rect(&gfx, 0, win_h - 16, win_w, 16, 0x00DDDDDD);
    if (blocks_head) {
        gfx_draw_string(&gfx, 4, win_h - 16, "Ready", COLOR_BLACK, 0x00DDDDDD);
    } else if (current_url[0]) {
        gfx_draw_string(&gfx, 4, win_h - 16, "Loading...", COLOR_BLACK, 0x00DDDDDD);
    } else {
        gfx_draw_string(&gfx, 4, win_h - 16, "Enter URL and press Enter", COLOR_BLACK, 0x00DDDDDD);
    }

    k->window_invalidate(window_id);
}

// Resolve a potentially relative URL against the current URL
static void resolve_url(const char *href, char *out, int max_len) {
    if (str_eqn(href, "http://", 7) || str_eqn(href, "https://", 8)) {
        // Absolute URL
        str_ncpy(out, href, max_len - 1);
        return;
    }

    // Parse current URL to get host and scheme
    url_t base;
    if (parse_url(current_url, &base) < 0) {
        str_ncpy(out, href, max_len - 1);
        return;
    }

    char *p = out;
    char *end = out + max_len - 1;

    // Build scheme://host:port (preserve https if current page is https)
    const char *s = base.use_tls ? "https://" : "http://";
    while (*s && p < end) *p++ = *s++;
    s = base.host;
    while (*s && p < end) *p++ = *s++;

    // Add port if non-default
    int default_port = base.use_tls ? 443 : 80;
    if (base.port != default_port) {
        *p++ = ':';
        // Convert port to string
        char port_str[8];
        int port = base.port;
        int i = 0;
        do {
            port_str[i++] = '0' + (port % 10);
            port /= 10;
        } while (port > 0);
        // Reverse and copy
        while (i > 0 && p < end) {
            *p++ = port_str[--i];
        }
    }

    if (href[0] == '/') {
        // Absolute path
        s = href;
        while (*s && p < end) *p++ = *s++;
    } else {
        // Relative path - append to current directory
        // Find last / in current path
        int last_slash = 0;
        for (int i = 0; base.path[i]; i++) {
            if (base.path[i] == '/') last_slash = i;
        }
        // Copy path up to and including last /
        for (int i = 0; i <= last_slash && p < end; i++) {
            *p++ = base.path[i];
        }
        // Append relative href
        s = href;
        while (*s && p < end) *p++ = *s++;
    }
    *p = '\0';
}

static void navigate_internal(const char *url, int add_to_history);

static void go_back(void) {
    if (history_pos > 0) {
        history_pos--;
        navigate_internal(history[history_pos], 0);
    }
}

static void navigate(const char *url) {
    // Add to history
    if (history_pos < MAX_HISTORY - 1) {
        history_pos++;
        str_ncpy(history[history_pos], url, 511);
        history_len = history_pos + 1;
    }
    navigate_internal(url, 1);
}

static void navigate_internal(const char *url, int add_to_history) {
    (void)add_to_history;
    str_cpy(current_url, url);
    str_cpy(url_input, url);
    free_blocks();
    scroll_offset = 0;
    draw_browser();

    url_t parsed;
    if (parse_url(url, &parsed) < 0) {
        add_block("Error: Invalid URL", 18, 1, 0, 0, 0, 0);
        draw_browser();
        return;
    }

    char *response = k->malloc(131072);  // 128KB
    if (!response) {
        add_block("Error: Out of memory", 20, 1, 0, 0, 0, 0);
        draw_browser();
        return;
    }

    http_response_t resp;
    int redirects = 0;

    while (1) {
        int len = http_get(&parsed, response, 131072, &resp);

        if (len <= 0) {
            add_block("Error: No response from server", 30, 1, 0, 0, 0, 0);
            break;
        }

        if (is_redirect(resp.status_code) && resp.location[0] && redirects < 5) {
            redirects++;
            // Check if it's a relative URL (starts with /)
            if (resp.location[0] == '/') {
                // Just update path, keep same host and protocol
                str_cpy(parsed.path, resp.location);
            } else {
                // Parse new URL (might switch http->https or vice versa)
                parse_url(resp.location, &parsed);
            }
            continue;
        }

        // For non-200 responses, still try to render the body (many sites return HTML error pages)
        if (resp.header_len > 0 && resp.header_len < len) {
            parse_html(response + resp.header_len, len - resp.header_len);
        }
        break;
    }

    k->free(response);
    draw_browser();
}

int main(kapi_t *kapi, int argc, char **argv) {
    k = kapi;

    if (!k->window_create) {
        k->puts("Browser requires desktop environment\n");
        return 1;
    }

    // Create window
    window_id = k->window_create(50, 50, WIN_WIDTH, WIN_HEIGHT, "VibeOS Browser");
    if (window_id < 0) {
        k->puts("Failed to create window\n");
        return 1;
    }

    win_buf = k->window_get_buffer(window_id, &win_w, &win_h);
    if (!win_buf) {
        k->window_destroy(window_id);
        return 1;
    }

    // Setup graphics context
    gfx_init(&gfx, win_buf, win_w, win_h, k->font_data);

    // Navigate to initial URL if provided
    if (argc > 1) {
        str_cpy(url_input, argv[1]);
        navigate(argv[1]);
    } else {
        str_cpy(url_input, "http://");
        cursor_pos = 7;
        editing_url = 1;
    }

    draw_browser();

    // Event loop
    int running = 1;
    while (running) {
        int event_type, data1, data2, data3;
        while (k->window_poll_event(window_id, &event_type, &data1, &data2, &data3)) {
            switch (event_type) {
                case WIN_EVENT_CLOSE:
                    running = 0;
                    break;

                case WIN_EVENT_KEY: {
                    int key = data1;

                    if (editing_url) {
                        if (key == '\n' || key == '\r') {
                            // Navigate
                            editing_url = 0;
                            navigate(url_input);
                        } else if (key == 27) {  // Escape
                            editing_url = 0;
                            str_cpy(url_input, current_url);
                            draw_browser();
                        } else if (key == '\b' || key == 127) {
                            if (cursor_pos > 0) {
                                // Delete character before cursor
                                for (int i = cursor_pos - 1; url_input[i]; i++) {
                                    url_input[i] = url_input[i + 1];
                                }
                                cursor_pos--;
                                draw_browser();
                            }
                        } else if (key == KEY_LEFT) {
                            if (cursor_pos > 0) cursor_pos--;
                            draw_browser();
                        } else if (key == KEY_RIGHT) {
                            if (url_input[cursor_pos]) cursor_pos++;
                            draw_browser();
                        } else if (key >= 32 && key < 127) {
                            int len = str_len(url_input);
                            if (len < 500) {
                                // Insert character at cursor
                                for (int i = len + 1; i > cursor_pos; i--) {
                                    url_input[i] = url_input[i - 1];
                                }
                                url_input[cursor_pos++] = key;
                                draw_browser();
                            }
                        }
                    } else {
                        // Not editing URL
                        if (key == 'g' || key == 'G') {
                            // Go to URL
                            editing_url = 1;
                            cursor_pos = str_len(url_input);
                            draw_browser();
                        } else if (key == 'r' || key == 'R') {
                            // Reload
                            navigate_internal(current_url, 0);
                        } else if (key == '\b' || key == 127 || key == 'b' || key == 'B') {
                            // Back
                            go_back();
                        } else if (key == KEY_UP || key == 'k') {
                            scroll_offset -= CHAR_H * 3;
                            if (scroll_offset < 0) scroll_offset = 0;
                            draw_browser();
                        } else if (key == KEY_DOWN || key == 'j') {
                            int max_scroll = content_height - (win_h - CONTENT_Y);
                            if (max_scroll < 0) max_scroll = 0;
                            scroll_offset += CHAR_H * 3;
                            if (scroll_offset > max_scroll) scroll_offset = max_scroll;
                            draw_browser();
                        } else if (key == ' ') {
                            // Page down
                            int max_scroll = content_height - (win_h - CONTENT_Y);
                            if (max_scroll < 0) max_scroll = 0;
                            scroll_offset += win_h - CONTENT_Y - CHAR_H * 2;
                            if (scroll_offset > max_scroll) scroll_offset = max_scroll;
                            draw_browser();
                        }
                    }
                    break;
                }

                case WIN_EVENT_MOUSE_DOWN: {
                    int mx = data1;
                    int my = data2;

                    // Click in address bar area
                    if (my < ADDR_BAR_HEIGHT) {
                        if (mx >= 4 && mx < 4 + BACK_BTN_W) {
                            // Back button clicked
                            go_back();
                        } else {
                            // URL bar clicked
                            editing_url = 1;
                            cursor_pos = str_len(url_input);
                            draw_browser();
                        }
                    } else if (scrollbar_h > 0 && mx >= win_w - SCROLLBAR_W) {
                        // Click on scrollbar area
                        if (my >= scrollbar_y && my < scrollbar_y + scrollbar_h) {
                            // Start dragging scrollbar
                            dragging_scrollbar = 1;
                            drag_start_y = my;
                            drag_start_scroll = scroll_offset;
                        } else if (my < scrollbar_y) {
                            // Click above scrollbar - page up
                            scroll_offset -= (win_h - CONTENT_Y - 16);
                            if (scroll_offset < 0) scroll_offset = 0;
                            draw_browser();
                        } else {
                            // Click below scrollbar - page down
                            int max_scroll = content_height - (win_h - CONTENT_Y - 16);
                            if (max_scroll < 0) max_scroll = 0;
                            scroll_offset += (win_h - CONTENT_Y - 16);
                            if (scroll_offset > max_scroll) scroll_offset = max_scroll;
                            draw_browser();
                        }
                    } else if (!editing_url) {
                        // Check for link click
                        for (int i = 0; i < num_link_regions; i++) {
                            link_region_t *lr = &link_regions[i];
                            if (mx >= lr->x && mx < lr->x + lr->w &&
                                my >= lr->y && my < lr->y + lr->h) {
                                // Clicked on a link!
                                char resolved[512];
                                resolve_url(lr->url, resolved, 512);
                                navigate(resolved);
                                break;
                            }
                        }
                    }
                    break;
                }

                case WIN_EVENT_MOUSE_UP:
                    dragging_scrollbar = 0;
                    break;

                case WIN_EVENT_MOUSE_MOVE:
                    if (dragging_scrollbar) {
                        int dy = data2 - drag_start_y;
                        int content_area = win_h - CONTENT_Y - 16;
                        int max_scroll = content_height - content_area;
                        if (max_scroll > 0 && content_area > scrollbar_h) {
                            int scroll_range = content_area - scrollbar_h;
                            scroll_offset = drag_start_scroll + dy * max_scroll / scroll_range;
                            if (scroll_offset < 0) scroll_offset = 0;
                            if (scroll_offset > max_scroll) scroll_offset = max_scroll;
                            draw_browser();
                        }
                    }
                    break;

                case WIN_EVENT_RESIZE:
                    // Re-fetch buffer with new dimensions
                    win_buf = k->window_get_buffer(window_id, &win_w, &win_h);
                    gfx_init(&gfx, win_buf, win_w, win_h, k->font_data);
                    draw_browser();
                    break;
            }
        }

        k->yield();
    }

    free_blocks();
    k->window_destroy(window_id);
    return 0;
}
