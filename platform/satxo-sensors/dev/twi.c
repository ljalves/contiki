#include <avr/io.h>
#include <util/twi.h>
#include "dev/twi.h"

void twi_init(uint8_t clk_rate)
{
  /* set clock rate */
  /* prescaler = 1 */
  TWSR = 0;

  /* clear interrupt flag and disable interrupts */
  TWCR &= ~_BV(TWIE);
  TWCR |= _BV(TWINT) | _BV(TWEN);

  /* baud rate */
  switch (clk_rate) {
  case TWI_CLK100:
    TWBR = (F_CPU / 100000 - 16) / 2;
    break;
  case TWI_CLK400:
    TWBR = (F_CPU / 400000 - 16) / 2;
    break;
  default:
    return;
  }
}

int8_t twi_transfer(struct twi_msg *msg)
{
  uint8_t twcr;
  int8_t i = -2;

  /* send start condition */
  TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
  /* wait */
  while (!(TWCR & _BV(TWINT)));

  /* check error */
  if (TW_STATUS != TW_START)
    goto error;

  /* send slave address */
  if (msg->flags & TWI_M_RD)
    TWDR = msg->addr | TW_READ;
  else
    TWDR = msg->addr | TW_WRITE;

  /* start tx */
  TWCR = _BV(TWINT) | _BV(TWEN);
  /* wait */
  while (!(TWCR & _BV(TWINT)));

  /* check ACK received */
  if (TW_STATUS != ((msg->flags & TWI_M_RD) ? TW_MR_SLA_ACK : TW_MT_SLA_ACK)) {
    i = -3;
    goto error;
  }

  if (msg->flags == 0) {
    /* write mode */

    /* send data buffer */
    for (i = 0; i < msg->len; i++) {
      TWDR = msg->buf[i];
      TWCR = _BV(TWINT) | _BV(TWEN);
      /* wait */
      while (!(TWCR & _BV(TWINT))); 

      /* get ACK status */
      if (TW_STATUS != TW_MT_DATA_ACK) {
        i = -4;
        goto error;
      }
    }

  } else if (msg->flags & TWI_M_RD) {
    /* read mode */

    twcr = _BV(TWINT) | _BV(TWEN) | _BV(TWEA);
    for (i = 0; i < msg->len; i++) {
      if (i == msg->len-1)
        twcr = _BV(TWINT) | _BV(TWEN);

      TWCR = twcr;
      /* wait rx */
      while (!(TWCR & _BV(TWINT)));

      msg->buf[i] = TWDR;

      /* get ACK status */
      if (TW_STATUS == TW_MR_DATA_NACK) {
        break;
      } else if (TW_STATUS != TW_MR_DATA_ACK) {
        i = -5;
        goto error;
      }
    }
  }

error:
  TWCR = _BV(TWINT)| _BV(TWEN)| _BV(TWSTO);
  /* wait for STOP to finish */
  while (TWCR & _BV(TWSTO));

  /* disable TWI */
  TWCR &= ~_BV(TWEN);
  return i+1;
}

