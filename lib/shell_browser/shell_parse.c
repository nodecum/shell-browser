#include <shell_browser/shell_parse.h>
#include <shell_browser/strbuf.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
LOG_MODULE_REGISTER(shell_parse, LOG_LEVEL_DBG);

/**
  Parsing the shell output
  ------------------------
  We characterize our approach
  - the shell prompt followed by a space is the indicator for
    a following command and or alternatives in the next,
    this line is the command line
  - alternatives start on a new line with spaces imediatly after
    the command line
  - all other text is output
  
*/

extern size_t shell_parse( const char* data, size_t length, 
			   struct shell_parse_t *p)
{
  int j = 0;
  for(; j < length; ++j) {
    // check the current character
    char c = data[j];
    bool separator = false;
    if( c == 0x1b) { p->vt = Escape; continue; }
    if( p->vt == Escape && c == '[') { p->vt = EscapeBracket; continue; }
    if( p->vt == EscapeBracket ) {
      p->vt = Unspec;
      if( c == 'm' ) continue; // MODESOFF 
      else if( c >= '0' && c <= '9') { p->vt = EscapeNumArg; continue; }
      else if( c == 'H' ) { 
        // clear screen, we just declare it as newline 
        //p->vt = Newline;
        //p->ct = AltOrOut; 
        // continue;
        c = ' ';
      } else continue;
    }
    if( p->vt == EscapeNumArg ) {
      if( c >= '0' && c <= '9')	continue;
      if( c == 'C' || c == 'D' ) { // DIRECTION
	      if( p->ct == AltOrOut || p->ct == Alt)
	        separator = true;
      } 
      else { p->vt = Unspec; continue; }
    }
    if( c == ' ' && (p->ct == AltOrOut || p->ct == Alt))
      { separator = true; } 
    // mode specific detection
    if( c == '\r' && ( p->ct == Cmd || p->ct == Alt || p->ct == Out))
      { p->vt = Return; continue; }
    if( c == '\n' && p->vt == Return) {
      p->vt = Newline;
      if( p->ct == Cmd || p->ct == AltOrOut) {
        p->ct = AltOrOut;  continue; 
      }
      //else if( p->ct == Alt ) { separator = true; }
      //else { } // Out mode, just use c as '\n' 
    }
    // following separators do not accumulate
    if( p->vt == Separator && separator) continue;  
    if( p->ct == PromptMatched) {
      if( c == '\r') {
        // return after Prompt
        p->ct = AltOrOut; 
        p->vt = Return;
        continue;
      } else {
        // change to Cmd context 
	      p->ct = Cmd; 
        // reset alternatives
        // p->alt->size=0; p->alt_pos=0;
      }
    }
    // test for prompt
    if( (p->vt == Newline && p->prompt_i == 0) || p->ct == Prompt ) {      
      const char c_ = p->prompt[ p->prompt_i];
      if( c_ == c) {
	      // charater matched, advance to next
        //
        // here is the point that we have to check if we
        // finished an alternative, if so then remove trailing
        // space if there is one
        // ( propably there is always one )
        if( p->ct == Alt && p->alt->size > 0 && p->alt->buffer[ p->alt->size-1] == ' ')
          --(p->alt->size);
        // we check if we matched the last prompt char
        if( p->prompt[p->prompt_i+1] == '\0') {
          p->prompt_i = 0;
          p->cmd->size = 0;
          if( p->alt_or_out_before_prompt) {
            // in case we had no output 
            // and no alternatives
            SHELL_PARSE_RESET_ALT( p);
          }
          p->ct = PromptMatched;
        } else {
          if( p->ct != Prompt) {
            p->alt_or_out_before_prompt = ( p->ct == AltOrOut);
            p->ct = Prompt;
          }
          ++(p->prompt_i);
        }
        p->vt = Unspec;
        continue;
      } else {
	      // character mismatch, 
        if( p->ct == AltOrOut || p->ct == Alt) {
          // do nothing if we are in Alt or AltOrOut Mode
        } else {
          // write already matched chars to
	        // Out Buffer and change to Out context
	        // the mismatched char will be read in the Out context handling
      	  size_t i = 0;
	        while( i < p->prompt_i) {
	          p->out->buffer[ p->out->size] = p->prompt[ i++];
	        }
          p->ct = Out;
          // reset alternatives and command line 
          SHELL_PARSE_RESET_ALT( p);
          p->cmd->size = 0;
        }
        p->prompt_i = 0;
      }
    }
    if( p->ct == AltOrOut ) {
      if( separator) {
	      // we change to alternatives mode
	      SHELL_PARSE_RESET_ALT( p);
        p->ct = Alt;
        // clear output 
        p->out->size = 0;
      } else {
	      // new line was not followed by separtor, this means output
	      p->out->size = 0; p->ct = Out;
      }
    }
    // fill the buffers accordingly to their context
    if( p->ct == Cmd) {
      p->cmd->buffer[ p->cmd->size] = c; STR_BUF_INC( p->cmd);
    } else if( p->ct == Out) {
      p->out->buffer[ p->out->size] = c; STR_BUF_INC( p->out);
    } else if( p->ct == Alt) {
      if( separator || p->vt == Newline) {
        if( p->alt->size > 0 && p->alt->buffer[ p->alt->size - 1] != ' ') {
          // spaces should not accumulate, no space at the beginning
          p->alt->buffer[ p->alt->size] = ' '; STR_BUF_INC( p->alt);    
        }
      } else {
        p->alt->buffer[ p->alt->size] = c; 
        if( p->alt->size == p->alt_eq_part) {
          // we are filling the first alternative
          ++(p->alt_eq_part);
        } else if( p->alt_eq_part > 0) {
          // we get the char position in the current alternative
          // by looking back to the space separator
          int i = p->alt->size - 1;
          while ( i > 0 && p->alt->buffer[ i] != ' ' ) --i;
          // now i points to the space char
          int rel_pos = p->alt->size - i - 1; // relative char pos in current alternativ
          // test if char matches
          if( p->alt_eq_part > rel_pos && p->alt->buffer[rel_pos] != c) 
            p->alt_eq_part = rel_pos;
        }
        STR_BUF_INC( p->alt);
      }
    }
    if( separator ) p->vt = Separator; 
    else if ( ! (c == '\n' && p->vt == Newline)) {
      // we can overwrite this 
      // if we have not a fresh newline
      p->vt = Unspec; 
    }  
  }   
  p->cmd->buffer[ p->cmd->size] = '\0';
  p->alt->buffer[ p->alt->size] = '\0';
  p->out->buffer[ p->out->size] = '\0';
  return j;
} 

