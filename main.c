/**
 * @file main.c
 * @author jdf
 * @date 2026-02-16
 * @brief Listen for a rising edge on PB2 (INT0) and toggle a relay connected to
 * PB0/PB1 accordingly.
 */

#ifndef F_CPU
#define F_CPU 1000000UL // 1 MHz (default ATtiny85 internal oscillator)
#endif

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <stdbool.h>
#include <util/delay.h>

// Relay control pins
#define RELAY_SET PB0
#define RELAY_RESET PB1

// Relay state variable
volatile bool relay_state = false;

// Interrupt handler for INT0 (PB2) - wakes from sleep
ISR(INT0_vect) {
  relay_state = !relay_state;

  uint8_t pin = relay_state ? RELAY_SET : RELAY_RESET;

  // Pulse appropriate pin high for 100ms
  PORTB |= (1 << pin);
  _delay_ms(100);
  PORTB &= ~(1 << pin);
}

int main() {
  // Configure relay control pins as outputs
  DDRB |= (1 << RELAY_SET) | (1 << RELAY_RESET);
  // Initialize both relay pins low
  PORTB &= ~((1 << RELAY_SET) | (1 << RELAY_RESET));

  // Configure INT0 (PB2) for rising edge interrupt
  MCUCR =
      (MCUCR & ~((1 << ISC01) | (1 << ISC00))) | (1 << ISC01) | (1 << ISC00);
  GIMSK |= (1 << INT0); // Enable INT0

  // Set sleep mode to idle (lowest power consumption while keeping timers
  // running)
  set_sleep_mode(SLEEP_MODE_IDLE);

  // Enable interrupts globally
  sei();

  // Sleep until interrupt on rising edge
  while (1) {
    sleep_enable();
    sleep_cpu();
  }

  return 0;
}
