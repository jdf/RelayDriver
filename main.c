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

// Relay control pins
#define RELAY_SET PB0
#define RELAY_RESET PB1

// Pulse duration: 100ms at 1MHz with prescaler 64 = ~156 Timer0 overflows
// (1000000 / 64 / 256) * 0.1 = 61, use 62 for margin
#define CLOCKS_PER_RELAY_PULSE 62

#define PRESCALE_BITS ((1 << CS01) | (1 << CS00)) // Timer0 prescaler 64
#define TIMER0_MASK (1 << TOIE0) // Timer0 overflow interrupt enable bit

volatile uint8_t clock_counter = 0;
volatile uint8_t active_pin_out = RELAY_RESET;

// Start Timer0 with prescaler 64
void start_timer() {
  TCCR0B |= PRESCALE_BITS;
  TIMSK |= TIMER0_MASK; // Enable Timer0 OVF interrupt
}

// Stop Timer0
void stop_timer() {
  TCCR0B &= ~PRESCALE_BITS;
  TIMSK &= ~TIMER0_MASK; // Disable Timer0 OVF interrupt
}

volatile bool service_switch_interrupt = false;
volatile bool service_timer_interrupt = false;

// Interrupt handler for INT0 (PB2)
ISR(INT0_vect) { service_switch_interrupt = true; }

// Interrupt handler for Timer0 overflow
ISR(TIM0_OVF_vect) { service_timer_interrupt = true; }

void service_switch() {
  if (active_pin_out == RELAY_SET) {
    active_pin_out = RELAY_RESET;
  } else {
    active_pin_out = RELAY_SET;
  }

  // Set pin high and start timer
  PORTB |= (1 << active_pin_out);
  clock_counter = CLOCKS_PER_RELAY_PULSE;
  start_timer();
}

void service_timer() {
  if (clock_counter > 0) {
    clock_counter--;
  } else {
    // Pulse complete, clear pin and stop timer
    PORTB &= ~(1 << active_pin_out);
    stop_timer();
  }
}

int main() {
  // Configure relay control pins as outputs
  DDRB |= (1 << RELAY_SET) | (1 << RELAY_RESET);
  // Initialize both relay pins low
  PORTB &= ~((1 << RELAY_SET) | (1 << RELAY_RESET));

  // Configure INT0 (PB2) for rising edge interrupt
  MCUCR |= (1 << ISC01) | (1 << ISC00);
  GIMSK |= (1 << INT0); // Enable INT0

  // Set sleep mode to idle (lowest power consumption while keeping timers
  // running)
  set_sleep_mode(SLEEP_MODE_IDLE);

  // Enable interrupts globally
  sei();

  while (1) {
    sleep_mode();
    if (service_switch_interrupt) {
      service_switch_interrupt = false;
      service_switch();
    } else if (service_timer_interrupt) {
      service_timer_interrupt = false;
      service_timer();
    }
  }

  return 0;
}
